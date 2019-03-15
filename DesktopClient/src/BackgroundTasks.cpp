#include "BackgroundTasks.hpp"
#include <cassert>
#include <QDebug>





////////////////////////////////////////////////////////////////////////////////
// BackgroundTasks:

Q_DECLARE_METATYPE(BackgroundTasks::TaskPtr);

BackgroundTasks::BackgroundTasks()
{
	qRegisterMetaType<BackgroundTasks::TaskPtr>();
	for (int i = QThread::idealThreadCount(); i > 0; --i)
	{
		auto executor = std::make_unique<Executor>(*this);
		auto executorPtr = executor.get();
		executor->setObjectName(QString("BackgroundTasks::Executor::%1").arg(i));
		mExecutors.push_back(std::move(executor));
		executorPtr->start();
	}
}





BackgroundTasks::~BackgroundTasks()
{
	assert(mShouldTerminate.load());  // App must explicitly call stopAll() before exiting;
}





BackgroundTasks & BackgroundTasks::get()
{
	static BackgroundTasks instance;
	return instance;
}





void BackgroundTasks::addTask(TaskPtr a_Task, bool a_Prioritize)
{
	{
		QMutexLocker lock(&mMtx);
		if (a_Prioritize)
		{
			mTasks.push_front(a_Task);
		}
		else
		{
			mTasks.push_back(a_Task);
		}
	}
	emit taskAdded(a_Task);
	mWaitForTasks.wakeOne();
}





void BackgroundTasks::enqueue(
	const QString & aTaskName,
	std::function<void ()> aTask,
	bool aPrioritize,
	std::function<void ()> aOnAbort
)
{
	/** Adapter between a Task class and two functions. */
	class FunctionTask: public BackgroundTasks::Task
	{
		using Super = BackgroundTasks::Task;

	public:

		FunctionTask(const QString & aFnName, std::function<void ()> aFnTask, std::function<void ()> aFnOnAbort):
			Super(aFnName),
			mTask(aFnTask),
			mOnAbort(aFnOnAbort)
		{
		}

		virtual void execute() override
		{
			mTask();
		}

		virtual void abort() override
		{
			mOnAbort();
			Super::abort();
		}

	protected:
		std::function<void ()> mTask;
		std::function<void ()> mOnAbort;
	};

	// Enqueue the task adapter:
	BackgroundTasks::get().addTask(std::make_shared<FunctionTask>(aTaskName, aTask, aOnAbort), aPrioritize);
}





const std::list<BackgroundTasks::TaskPtr> BackgroundTasks::tasks() const
{
	QMutexLocker lock(&mMtx);
	std::list<TaskPtr> res(mTasks);
	return res;
}





void BackgroundTasks::stopAll()
{
	// Tell all executors to terminate:
	qDebug() << "Terminating all executors...";
	mShouldTerminate = true;
	mWaitForTasks.wakeAll();

	// Wait for all executors to terminate:
	qDebug() << "Waiting for all executors...";
	for (auto & e: mExecutors)
	{
		e->wait();
	}

	// Abort all tasks left over in the queue:
	qDebug() << "Aborting non-executed tasks.";
	for (auto & t: mTasks)
	{
		t->abort();
		emit taskAborted(t);
	}
}





BackgroundTasks::TaskPtr BackgroundTasks::getNextTask()
{
	QMutexLocker lock(&mMtx);
	if (mShouldTerminate)
	{
		return nullptr;
	}
	while (mTasks.empty())
	{
		if (mShouldTerminate)
		{
			return nullptr;
		}
		mWaitForTasks.wait(&mMtx);
	}
	auto task = mTasks.front();
	mTasks.pop_front();
	return task;
}





void BackgroundTasks::emitTaskFinished(BackgroundTasks::TaskPtr a_Task)
{
	emit taskFinished(a_Task);
}





////////////////////////////////////////////////////////////////////////////////
// BackgroundTasks::Executor:

BackgroundTasks::Executor::Executor(BackgroundTasks & a_Parent):
	m_Parent(a_Parent)
{
}




void BackgroundTasks::Executor::run()
{
	auto task = m_Parent.getNextTask();
	while (task != nullptr)
	{
		task->execute();
		QMetaObject::invokeMethod(
			&m_Parent,
			"emitTaskFinished",
			Qt::QueuedConnection,
			Q_ARG(BackgroundTasks::TaskPtr, task)
		);
		task = m_Parent.getNextTask();
	}
}




