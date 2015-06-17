/* plotwindow.h
 * ------------
 * Header file for the data plotting window for the meaview application.
 * (C) 2015 Benjamin Naecker bnaecker@stanford.edu
 */

#ifndef _PLOTWINDOW_H_
#define _PLOTWINDOW_H_

/* C++ includes */

/* Qt includes */
#include <QGridLayout>
#include <QList>
#include <QThread>
#include <QSemaphore>
#include <QPoint>
#include <QSet>
#include <QStringList>
#include <QPair>

/* meaview includes */
#include "qcustomplot.h"
#include "settings.h"
#include "plotworker.h"
#include "channelinspector.h"
#include "h5recording/include/h5recording.h"

class PlotWindow : public QWidget {
	Q_OBJECT

	public:
		PlotWindow(int nrows, int ncols, QWidget *parent = 0);
		~PlotWindow();

		void plotData(H5Rec::Samples &s);
		void waitAll(void);
		void forceReplot(void);

	signals:
		void sendData(QSemaphore *sem, int channel, QString label, 
				QCPGraph *subplot, QVector<double> *data, bool isClicked);
		void allSubplotsUpdated(QSemaphore *sem,
				const int nthreads, QCustomPlot *p);

	public slots:
		void toggleVisible(void);
		void clearAll(void);
		void countPlotsUpdated(void);
		void createChannelInspector(QMouseEvent *event);
		void handleChannelClick(QMouseEvent *event);
		void updateChannelView(void);
		void blockResize(void);
		void unblockResize(void);

	private:
		void initThreadPool();
		void initPlotGroup();
		int findSubplotClicked(QPoint pos);
		void removeChannelInspector(int channel);

		const unsigned int numThreads = QThread::idealThreadCount();
		int nrows;
		int ncols;
		int numPlotsUpdated = 0;
		QStringList channelLabels;
		QList<QPair<int, int> > channelView;
		QSet<int> clickedPlots;
		QSet<ChannelInspector *> channelInspectors;
		Settings settings;
		QGridLayout *layout;
		QCustomPlot *plot;
		QList<QCPGraph *> subplotList;
		QList<QThread *> threadList;
		QList<PlotWorker *> workerList;
		QSemaphore *sem;
};

#endif

