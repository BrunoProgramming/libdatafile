/* h5recording.h
 * Public API for the H5Recording class, which represents a data file
 * to which experiments are saved.
 *
 * (C) 2015 Benjamin Naecker bnaecker@stanford.edu
 */

#ifndef _H5RECORDING_H_
#define _H5RECORDING_H_

/* C++ standard library */
#include <string>
#include <vector>

/* Third-party includes */
#include "H5Cpp.h"
#include "boost/multi_array.hpp"

using namespace std;
using namespace H5;

/* Constants */
const string RECORDING_FILE_EXTENSION = ".h5";
const int DATASET_RANK = 2;
const int NCHANNELS = 64;
const int BLOCK_SIZE = 2000;
const hsize_t DATASET_DEFAULT_DIMS[DATASET_RANK] = {NCHANNELS, BLOCK_SIZE};
const hsize_t DATASET_CHUNK_DIMS[DATASET_RANK]= {NCHANNELS, BLOCK_SIZE};

/* Types */
typedef boost::multi_array<int16_t, 2> samples;
typedef boost::multi_array<double, 2> samples_d;

/* Forward declaration of MealogWindow class, which is the
 * GUI class that streams data from the NI-DAQ server and writes
 * to disk. It is declared as a friend of the H5Recording, giving
 * it access to its private write methods.
 */
class MealogWindow;

class H5Recording {
	friend class MealogWindow;

	public:
		/* Construct a new H5Recording object, either from an existing
		 * H5 file or a new file.
		 */
		H5Recording(string filename);
		~H5Recording();

		string filename(void);			// Returns the full pathname of the file
		double length(void);			// Returns the recording's length in seconds
		int16_t type(void);				// Returns the traditional bin file type
		int16_t version(void);			// Returns the traditional bin file version
		uint32_t nsamples(void);		// Returns the number of samples
		uint32_t nchannels(void); 		// Returns the number of channels
		bool live(void);				// Returns true if data is currently recorded
		uint32_t lastValidSample(void);	// Returns index of last sample written to disk
		uint32_t blockSize(void);		// Returns size of HDF5 file chunks. Name
										// 	comes from the binfile blocking arrangement
		float sampleRate(void);			// Returns the data sample rate
		float gain(void);				// Returns the NI-DAQ ADC gain
		float offset(void);				// Returns the NI-DAQ ADC offset
		string date(void);				// Returns date of the recording
		string time(void);				// Returns time of the recording
		string room(void);				// Returns room in which recording occurred

		/* These functions return the requested chunks of data,
		 * in various data types. Data is stored on disk as signed 16-bit integers,
		 * but can be returned as doubles, which will give the true voltage values.
		 *
		 * The data is stored in Boost multi-arrays, which have been typedef'd into
		 * the convenience type `samples`.
		 */
		samples data(int startSample, int endSample);
		void data(int startSample, int endSample, samples &data);
		void data(int startSample, int endSample, samples_d &data);

	private:
		H5File file;				// The actual HDF5 file
		DataSpace dataspace;		// Data space for actual data
		DataType datatype;			// Type for the actual data
		DSetCreatPropList props;	// Properties for the dataset (chunking, etc)
		DataSet dataset;			// The HDF5 dataset containing data
		bool readOnly;				// Protection

		string _filename;			// Full path name of HDF5 file
		bool _live;					// True if file is currently being written to
		int16_t _type = 2;			// Bin-file type
		int16_t _version = 1;		// Bin-file version
		double _length;				// Length of recording in seconds
		uint32_t _nsamples;			// Length of recording in samples
		uint32_t _nchannels = 64;	// Number of channels
		uint32_t _lastValidSample;	// Latest valid sample index
		uint32_t _blockSize = 20000;// Size of HDF5 chunks and bin-file blocks
		float _sampleRate = 10000;	// Data sample rate
		float _gain;				// NI-DAQ ADC gain
		float _offset;				// NI-DAQ ADC offset
		string _time;				// Time of recording start
		string _date;				// Date of recording
		string _room = "recorded in d239"; // Location of recording

		/* These functions implement the actual process of writing attributes
		 * of either the HDF5 data file or the dataset. Where applicable,
		 * they are used by the various setter functions below.
		 */
		void writeFileAttr(string name, const DataType &type, void *buf);
		void writeDataAttr(string name, const DataType &type, void *buf);
		void writeDataStringAttr(string name, string value);
		void writeAllAttributes(void);

		/* Write data to the file */
		void setData(int startSample, int endSample, samples &data);
		void setData(int startSample, int endSample, vector<vector<int16_t> > &data);

		/* Setters for data attributes */
		void setFilename(string filename);
		void setLength(double length);	
		void setLastValidSample(size_t sample);
		void setLive(bool live);
		void setFileType(int16_t type);
		void setFileVersion(int16_t version);
		void setSampleRate(float sampleRate);
		void setNumChannels(uint32_t nchannels);
		void setNumSamples(uint32_t nsamples);
		void setGain(float gain);
		void setOffset(float offset);
		void setBlockSize(size_t blockSize);
		void setDate(string date);
		void setTime(string time);
		void setRoom(string room);

		/* These functions implement the actual reading of HDF5 file
		 * or dataset attributes, used by the various "read" functions
		 * below.
		 */
		void readFileAttr(string name, void *buf);
		void readDataAttr(string name, void *buf);
		void readDataStringAttr(string name, string &dst);

		/* These functions do not return the value of the corresponding
		 * data member. They read the values saved in a file, and are thus
		 * most useful when loading an existing data file.
		 */
		void readIsLive(void);
		void readLastValidSample(void);
		void readFileType(void);
		void readFileVersion(void);
		void readSampleRate(void);
		void readBlockSize(void);
		void readNumSamples(void);
		void readNumChannels(void);
		void readGain(void);
		void readOffset(void);
		void readDate(void);
		void readTime(void);
		void readRoom(void);
};

#endif

