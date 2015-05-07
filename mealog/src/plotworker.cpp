/* plotworker.cpp
 * Implementation of the worker object that actually plots data
 * in separate threads for efficiency.
 *
 * (C) 2015 Benjamin Naecker bnaecker@stanford.edu
 */

#include "plotworker.h"

PlotWorker::PlotWorker(int id, QObject *parent) : QObject(parent) {
	this->id = id;
	getSettings();
	transferring = false;
}

PlotWorker::PlotWorker(const PlotWorker &other) {
	pen = QPen(other.pen);
	automean = other.automean;
	autoscale = other.autoscale;
	scale = other.scale;
	xData = QVector<double>(other.xData);
	transferring = false;
}

PlotWorker::~PlotWorker() {
}

void PlotWorker::transferPlotData(QSemaphore *sem, int workerId, 
		int channel, QCPGraph *subplot, QVector<double> *data) {

	if (workerId != id)
		return;

	transferring = true;
	getSettings();
	constructXData(data->size());

	if (automean) {
		double mean = 0.0;
		for (auto i = 0; i < data->size(); i++)
			mean += data->at(i);
		mean /= data->size();
		for (auto i = data->begin(); i != data->end(); i++)
			(*i) -= mean;
	}
	subplot->setPen(pen);
	sem->acquire();
	subplot->setData(xData, *data);
	if ( (autoscale) || RESCALED_CHANNELS.contains(channel) )
		subplot->valueAxis()->rescale();
	else
		subplot->valueAxis()->setRange(-scale * NEG_DISPLAY_RANGE,
				scale * POS_DISPLAY_RANGE);
	subplot->keyAxis()->rescale();
	sem->release();
	transferring = false;
	delete data;

	emit dataTransferDone(channel);
}

void PlotWorker::getSettings(void) {
	pen = settings.getPlotPen();
	automean = settings.getAutoMean();
	autoscale = settings.getAutoscale();
	scale = settings.getDisplayScale();
}

void PlotWorker::constructXData(int npoints) {
	if (xData.size() == npoints)
		return;
	xData.resize(npoints);
	std::iota(xData.begin(), xData.end(), 0.0);
}

void PlotWorker::replot(QSemaphore *sem, const int nthreads, 
		QCustomPlot *p) {
	sem->acquire(nthreads);
	p->replot();
	sem->release(nthreads);
}

