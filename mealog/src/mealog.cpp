/* mealog.cpp
 * This file implements the mealog application, a simple GUI program
 * to stream data from the NI-DAQ server running on the Windows machine,
 * and log the data to the localhost in HDF5 format.
 *
 * This application can be used to set parameters, start and stop 
 * recordings.
 *
 * (C) 2015 Benjamin Naecker bnaecker@stanford.edu
 */

#include <string>

#include <QApplication>
#include <QVariant>
#include <QDebug>
#include <QHostAddress>
#include <QMessageBox>
#include <QTime>
#include <QDate>
#include <QDataStream>
#include <QFileDialog>
#include <QErrorMessage>

#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent>

#include <cstdio>

#include "mealog.h"

#include <boost/multi_array.hpp>
using boost::extents;

MealogWindow::MealogWindow(QWidget *parent) : QMainWindow(parent) {
	initGui();
	initServer();
	initSignals();
}

MealogWindow::~MealogWindow() {
}

void MealogWindow::initGui(void) {
	this->setWindowTitle("Mealog");
	this->setGeometry(10, 10, 300, 200);
	mainLayout = new QGridLayout();

	nidaqGroup = new QGroupBox("NI-DAQ");
	nidaqLayout = new QGridLayout();
	connectButton = new QPushButton("Connect");
	connectButton->setToolTip("Connect to NI-DAQ server to initialize the recording");
	nidaqHostLabel = new QLabel("IP:");
	nidaqHostLabel->setAlignment(Qt::AlignRight);
	nidaqHost = new QLineEdit("127.0.0.1");
	nidaqHost->setToolTip("IP address of the computer running the NI-DAQ server");
	nidaqValidator = new QRegExpValidator(
			QRegExp("^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\\.){3}"\
				"([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])"));
	nidaqHost->setValidator(nidaqValidator);
	nidaqStatusLabel = new QLabel("Status:");
	nidaqStatusLabel->setAlignment(Qt::AlignRight);
	nidaqStatus = new QLabel("Not connected");
	nidaqStatus->setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
	nidaqStatus->setToolTip("Status of connection with NIDAQ");
	nidaqLayout->addWidget(connectButton, 0, 0);
	nidaqLayout->addWidget(nidaqHostLabel, 0, 1);
	nidaqLayout->addWidget(nidaqHost, 0, 2, 1, 2);
	nidaqLayout->addWidget(nidaqStatusLabel, 1, 0);
	nidaqLayout->addWidget(nidaqStatus, 1, 1, 1, 4);
	nidaqGroup->setLayout(nidaqLayout);

	ctrlGroup = new QGroupBox("Controls");
	ctrlLayout = new QGridLayout();
	initRecordingButton = new QPushButton("Initialize");
	initRecordingButton->setToolTip("Initialize a recording with the given parameters");
	startMeaviewButton = new QPushButton("Start Meaview");
	startMeaviewButton->setToolTip("Start Meaview application to visualize recording");
	if (checkMeaviewRunning())
		startMeaviewButton->setEnabled(false);
	startButton = new QPushButton("Start");
	startButton->setToolTip("Start recording");
	startButton->setEnabled(false);
	quitButton = new QPushButton("Quit");
	quitButton->setToolTip("Quit Mealog");
	ctrlLayout->addWidget(initRecordingButton, 0, 0);
	ctrlLayout->addWidget(startMeaviewButton, 0, 1);
	ctrlLayout->addWidget(startButton, 0, 2);
	ctrlLayout->addWidget(quitButton, 0, 3);
	ctrlGroup->setLayout(ctrlLayout);

	paramGroup = new QGroupBox("Recording parameters");
	paramLayout = new QGridLayout();

	timeLabel = new QLabel("Time:");
	timeLine = new QLineEdit(QString::number(Mealog::DEFAULT_EXPERIMENT_LENGTH));
	timeLine->setToolTip("Set duration of the recording");
	timeValidator = new QIntValidator(1, Mealog::MAX_EXPERIMENT_LENGTH);
	timeLine->setValidator(timeValidator);

	fileLabel = new QLabel("Data file:");
	fileLine = new QLineEdit(Mealog::DEFAULT_SAVE_FILE.fileName(), this);
	fileLine->setToolTip("Name of file to which data is written");
	fileValidator = new QRegExpValidator(QRegExp("(\\w+[-_]*)+"));
	fileLine->setValidator(fileValidator);

	savedirLabel = new QLabel("Save dir:");
	savedirLine = new QLineEdit(Mealog::DEFAULT_SAVE_DIR.absolutePath());
	savedirLine->setToolTip("Directory of current recording data file");
	savedirLine->setReadOnly(true);
	chooseDirButton = new QPushButton("Choose");
	chooseDirButton->setToolTip("Choose save directory");

	adcRangeBox = new QComboBox(this);
	for (auto &each : Mealog::ADC_RANGES)
		adcRangeBox->addItem(QString::number(each), QVariant(each));
	adcRangeLabel = new QLabel("ADC range:");
	adcRangeBox->setToolTip("Set the voltage range of the NI-DAQ card");
	adcRangeBox->setCurrentIndex(
			Mealog::ADC_RANGES.indexOf(Mealog::DEFAULT_ADC_RANGE));

	triggerBox = new QComboBox(this);
	triggerBox->addItems(Mealog::TRIGGERS);
	triggerLabel = new QLabel("Trigger:");
	triggerBox->setToolTip("Set the triggering mechanism for starting the experiment");

	paramLayout->addWidget(timeLabel, 1, 0);
	paramLayout->addWidget(timeLine, 1, 1);
	paramLayout->addWidget(fileLabel, 1, 2);
	paramLayout->addWidget(fileLine, 1, 3, 1, 2);
	paramLayout->addWidget(savedirLabel, 2, 0);
	paramLayout->addWidget(savedirLine, 2, 1, 1, 3);
	paramLayout->addWidget(chooseDirButton, 2, 4);
	paramLayout->addWidget(adcRangeLabel, 3, 0);
	paramLayout->addWidget(adcRangeBox, 3, 1);
	paramLayout->addWidget(triggerLabel, 3, 2);
	paramLayout->addWidget(triggerBox, 3, 3);
	paramGroup->setLayout(paramLayout);

	mainLayout->addWidget(nidaqGroup, 0, 0);
	mainLayout->addWidget(ctrlGroup, 1, 0);
	mainLayout->addWidget(paramGroup, 2, 0);

	statusBar = new QStatusBar(this);
	statusBar->showMessage("Ready");
	this->setStatusBar(statusBar);
	this->setCentralWidget(new QWidget(this));
	this->centralWidget()->setLayout(mainLayout);
}

void MealogWindow::initServer(void) {
	server = new QTcpServer(this);
	server->listen(QHostAddress(Mealog::IPC_HOST), Mealog::IPC_PORT);
	qDebug() << "Server started at: " << server->serverAddress().toString();
}

void MealogWindow::acceptClients(void) {
	QTcpSocket *socket = server->nextPendingConnection();
	socket->setParent(this);
	qDebug() << "New client: " << socket->peerAddress() << ":" << socket->peerPort();
	connect(socket, SIGNAL(readyRead()), 
			this, SLOT(respondToClient()));
}

bool MealogWindow::readMessage(QTcpSocket *socket, 
		mearec::RecordingStatusRequest &request) {
	/* Read the size */
	QDataStream stream(socket);
	quint32 size = 0;
	stream >> size;
	if (size == 0)
		return false;

	/* Return the parsed message */
	return request.ParseFromString(socket->read(size).toStdString());
}

bool MealogWindow::writeMessage(QTcpSocket *socket, 
		mearec::RecordingStatusReply &reply) {
	/* Write the size */
	QDataStream stream(socket);
	quint32 size = reply.ByteSize();
	qDebug() << "Writing size (" << size << ") to socket's datastream";
	stream << size;

	/* Write the message */
	std::string replyData;
	if (!reply.SerializeToString(&replyData))
		return false;
	if (socket->write(replyData.data(), size) != size)
		return false;
	socket->flush();
	return true;
}

void MealogWindow::respondToClient(void) {
	/* Ignore all messages until initialized? */
	if (!recordingInitialized) {
		qDebug() << "Ignoring message from peer, recording is not initialized";
		return;
	}
	QTcpSocket *socket = dynamic_cast<QTcpSocket *>(QObject::sender());
	mearec::RecordingStatusRequest request;
	bool ok = readMessage(socket, request);
	if (!ok) // ignore bad messages for now
		return;

	qDebug() << "Received request from peer: " << request.ByteSize() << " bytes";

	/* Send back the correct reply */
	mearec::RecordingStatusReply reply = constructStatusReply(request);
	qDebug() << "Sending reply: " << reply.ByteSize() << " bytes";
	writeMessage(socket, reply);
}

mearec::RecordingStatusReply MealogWindow::constructStatusReply(
		mearec::RecordingStatusRequest request) {
	mearec::RecordingStatusReply reply;
	if (request.has_status() && request.status())
		reply.set_status(recordingStatus);
	if (request.has_filename() && request.filename())
		reply.set_filename(recording->filename());
	if (request.has_length() && request.length())
		reply.set_length(recording->length());
	if (request.has_nsamples() && request.nsamples())
		reply.set_nsamples(recording->nsamples());
	if (request.has_lastvalidsample() && request.lastvalidsample())
		reply.set_lastvalidsample(recording->lastValidSample());
	if (request.has_blocksize() && request.blocksize())
		reply.set_blocksize(recording->blockSize());
	if (request.has_samplerate() && request.samplerate())
		reply.set_samplerate(recording->sampleRate());
	if (request.has_gain() && request.gain())
		reply.set_gain(recording->gain());
	if (request.has_offset() && request.offset())
		reply.set_offset(recording->offset());
	if (request.has_date() && request.date())
		reply.set_date(recording->date());
	return reply;
}

QFile *MealogWindow::getFullFilename(void) {
	QDir d(savedirLine->text());
	QFile *path = new QFile(QDir::cleanPath(d.absolutePath() + "/" + fileLine->text()));
	return path;
}

bool MealogWindow::removeOldRecording(QFile &path) {
	if (!path.remove()) {
		QMessageBox box(this);
		box.setText("Error");
		box.setInformativeText(
				"Could not overwrite the requested file. \
				Remove manually and try again.");
		box.setStandardButtons(QMessageBox::Ok);
		box.setDefaultButton(QMessageBox::Ok);
		box.exec();
		return false;
	}
	return true;
}

void MealogWindow::handleBrokenServerConnection(void) {
	if (numSamplesAcquired == client->nsamples()) {
		cleanupRecording();
	} else {
		/* Broken pipe! */
		connectButton->setText("Connect");
		nidaqStatus->setText("Connection to NI-DAQ server interrupted");
		nidaqHost->setEnabled(true);
		if (isRecording) {
			QErrorMessage *errMsg = new QErrorMessage(this);
			errMsg->showMessage("Connection to NI-DAQ server interrupted"\
					", recording terminated!");
		}
	}
}

void MealogWindow::cleanupRecording(void) {
	connectButton->setText("Connect");
	nidaqStatus->setText("Connection to NI-DAQ server ended");
	isRecording = false;
	statusBar->showMessage("Recording finished", 10000);
	startButton->setEnabled(false);
	delete this->recording;
}

void MealogWindow::setRecordingParameters(void) {
	/* Set length and sample information */
	recording->setLength(timeLine->text().toDouble());
	recording->setLive(true);
	recording->setLastValidSample(0);

	/* Use defaults */
	recording->setFileType(recording->type());
	recording->setFileVersion(recording->version());
	recording->setSampleRate(recording->sampleRate());

	recording->setNumSamples(recording->length() * recording->sampleRate());
	double adcRange = Mealog::ADC_RANGES.at(adcRangeBox->currentIndex());
	recording->setOffset(adcRange);
	recording->setGain((adcRange * 2) / (1 << 16));
	recording->setLastValidSample(0);

	/* Date and time */
	QString tm(QTime::currentTime().toString("h:mm:ss AP"));
	QString dt(QDate::currentDate().toString("ddd, MMM dd, yyyy"));
	recording->setTime(tm.toStdString());
	recording->setDate(dt.toStdString());
}

void MealogWindow::sendInitMsg(void) {
	client->setLength(timeLine->text().toFloat());
	client->setAdcRange(adcRangeBox->currentData().toFloat());
	client->setBlockSize(BLOCK_SIZE);
	client->setTrigger(triggerBox->currentText());
	client->initExperiment();
}

void MealogWindow::setParameterSelectionsEnabled(bool enabled) {
	adcRangeBox->setEnabled(enabled);
	triggerBox->setEnabled(enabled);
	timeLine->setEnabled(enabled);
	fileLine->setEnabled(enabled);
	savedirLine->setEnabled(enabled);
	chooseDirButton->setEnabled(enabled);
}

void MealogWindow::initRecording(void) {
	statusBar->showMessage("Initializing recording");
	QFile *path = getFullFilename();
	if (path->exists()) {  
		if (!(path->fileName().contains(Mealog::DEFAULT_SAVE_FILE.fileName()))) {
			if (!confirmFileOverwrite(*path))
				return;
		}
		if (!removeOldRecording(*path))
			return;
	}

	/* Make recording and set parameters from choices */
	recording = new H5Recording(path->fileName().toStdString());
	setRecordingParameters();
	recording->flush();
	if (client != nullptr)
		sendInitMsg();

	/* Disable parameter selections */
	setParameterSelectionsEnabled(false);

	/* Change init button to "un-init" */
	initRecordingButton->setText("Reset parameters");
	initRecordingButton->setToolTip("Destroy the current recording and reset parameters");
	disconnect(initRecordingButton, SIGNAL(clicked()), 
			this, SLOT(initRecording()));
	connect(initRecordingButton, SIGNAL(clicked()),
			this, SLOT(deInitRecording()));
	recordingInitialized = true;
	delete path;
	statusBar->showMessage("Ready");

	/* Enable "Start" button, if already connected to NIDAQ server */
	if ((client != nullptr) && (client->isConnected()))
		startButton->setEnabled(true);
}

void MealogWindow::deInitRecording(void) {
	statusBar->showMessage("Reseting recording");
	/* Close the recording file */
	delete recording;
	recording = nullptr;

	/* Remove the file */
	if (!removeOldRecording(*getFullFilename()))
		return;

	/* Re-enabled parameter selection */
	setParameterSelectionsEnabled(true);

	/* Change buttons */
	startButton->setEnabled(false);
	initRecordingButton->setText("Initialize");
	initRecordingButton->setToolTip("Initialize a recording with the given parameters");
	disconnect(initRecordingButton, SIGNAL(clicked()), 
			this, SLOT(deInitRecording()));
	connect(initRecordingButton, SIGNAL(clicked()),
			this, SLOT(initRecording()));
	recordingInitialized = false;
	statusBar->showMessage("Ready");
}

bool MealogWindow::confirmFileOverwrite(const QFile &path) {
	QMessageBox box(this);
	box.setWindowTitle("File exists"); // ignore on OSX
	box.setText("The selected file already exists.");
	box.setInformativeText("The file \"" + 
			path.fileName() + "\" already exists. Overwrite?");
	box.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
	box.setDefaultButton(QMessageBox::Cancel);
	return box.exec();
}

void MealogWindow::initSignals(void) {
	connect(this->quitButton, SIGNAL(clicked()), 
			qApp, SLOT(quit()));
	connect(this->startMeaviewButton, SIGNAL(clicked()), 
			this, SLOT(startMeaview()));
	connect(this->server, SIGNAL(newConnection()), 
			this, SLOT(acceptClients()));
	connect(this->initRecordingButton, SIGNAL(clicked()), 
			this, SLOT(initRecording()));
	connect(this->chooseDirButton, SIGNAL(clicked()),
			this, SLOT(chooseSaveDir()));
	connect(this->connectButton, SIGNAL(clicked()), 
			this, SLOT(connectToDaqsrv()));
	connect(this->startButton, SIGNAL(clicked()), 
			this, SLOT(startRecording()));
}

void MealogWindow::chooseSaveDir(void) {
	QString dir = QFileDialog::getExistingDirectory(this,
			"Choose save directory",
			savedirLine->text(),
			QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
	savedirLine->setText(dir);
}

bool MealogWindow::checkMeaviewRunning(void) {
	QProcess pgrep;
	pgrep.setProgram("pgrep");
	QStringList args = {"meaview"};
	pgrep.setArguments(args);
	pgrep.start();
	pgrep.waitForFinished();
	return (pgrep.readAllStandardOutput().length() > 0);
}

void MealogWindow::startMeaview(void) {
	statusBar->showMessage("Starting meaview");
	if (checkMeaviewRunning())
		return;
	QProcess m;
	m.startDetached("/Users/bnaecker/FileCabinet/stanford/baccuslab/mearec/meaview/meaview.app/Contents/MacOS/meaview");
	m.waitForStarted();
	statusBar->showMessage("Ready");
}

void MealogWindow::connectToDaqsrv(void) {
	statusBar->showMessage("Connecting to NI-DAQ server");
	nidaqStatus->setText("Connecting to NI-DAQ server");
	client = new DaqClient(nidaqHost->text());
	connectButton->setEnabled(false);
	connect(client, SIGNAL(connectionMade(bool)),
			this, SLOT(handleDaqsrvConnection(bool)));
	connect(client, SIGNAL(connectionBroken()), 
			this, SLOT(handleBrokenServerConnection()));
	client->connectToDaqsrv();
}

void MealogWindow::disconnectFromDaqsrv(void) {
	client->disconnectFromDaqsrv();
	connectButton->setText("Connect");
	nidaqStatus->setText("Not connected");
	nidaqHost->setEnabled(true);
	disconnect(connectButton, SIGNAL(clicked()), 
			this, SLOT(disconnectFromDaqsrv()));
	connect(connectButton, SIGNAL(clicked()),
			this, SLOT(connectToDaqsrv()));
	disconnect(client, SIGNAL(connectionBroken()), 
			this, SLOT(handleBrokenServerConnection()));
}

void MealogWindow::handleDaqsrvConnection(bool made) {
	if (made) {
		nidaqStatus->setText("Connected to NI-DAQ");
		connectButton->setText("Disconnect");
		connectButton->setEnabled(true);
		nidaqHost->setEnabled(false);
		disconnect(connectButton, SIGNAL(clicked()), 
				this, SLOT(connectToDaqsrv()));
		connect(connectButton, SIGNAL(clicked()),
				this, SLOT(disconnectFromDaqsrv()));
		if (recordingInitialized) {
			sendInitMsg();
			startButton->setEnabled(true);
		}
		statusBar->showMessage("Ready");
	} else {
		nidaqStatus->setText("Error connecting to NI-DAQ, correct IP?");
		delete client;
		client = nullptr;
		connectButton->setEnabled(true);
		statusBar->showMessage("Ready");
	}
}

void MealogWindow::startRecording(void) {
	connect(client, SIGNAL(dataAvailable()), this, SLOT(recvData()));
	client->startRecording();
	statusBar->showMessage(QString("Recording data to %1").arg(
				getFullFilename()->fileName()));
}

void MealogWindow::recvData(void) {
	H5Rec::samples samples(extents[client->nchannels()][client->blockSize()]);
	client->recvData(samples.origin());
	recording->setData(numSamplesAcquired, 
			numSamplesAcquired + client->blockSize(), samples);
	numSamplesAcquired += client->blockSize();
	recording->setLastValidSample(numSamplesAcquired);
	emit newDataAvailable();
	qDebug() << numSamplesAcquired << "acquired";
}
