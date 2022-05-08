#pragma once

#include <memory>
#include <map>
#include "Exception.hpp"





/** A collection of the base objects, so that they don't need to be pushed around in parameters.
The collection is built upon app startup and is expected not to change after its initial build.
Each component receives the ComponentCollection in its constructor, so it can store it for later queries.
Once all the components are created, they are then started; the order of the starting is specified by
the components' requests to ComponentCollection::requireForStart().

Usage:
At app start:
ComponentCollection cc
auto db = cc.addNew<Database>(...);
...
cc.start();

In regular operation:
auto db = ComponentCollection::get<Database>();
*/
class ComponentCollection
{
public:

	/** Specifies the kind of the individual component. */
	enum ComponentKind
	{
		ckInstallConfiguration,
		ckMultiLogger,                ///< The per-device and per-subsystem logger
		ckDatabase,
		ckDevices,
		ckUdpBroadcaster,
		ckTcpListener,
		ckDevicePairings,             ///< The storage of the device pairing data (DevPubID <-> {DevPubKey, LocalPubKey})
		ckDeviceBlacklist,            ///< The storage of device blacklist
		ckConnectionMgr,              ///< The list of all current device connections
		ckDetectedDevices,            ///< All the devices that have been detected by the enumerators
		ckUsbDeviceEnumerator,        ///< The background thread that monitors the connected USB devices
		ckBluetoothDeviceEnumerator,  ///< The background thread that monitors the available Bluetooth devices
	};


protected:

	/** An internal base for all components, that keeps track of the component kind.
	Needed so that all components can be put into a vector of same-type pointers. */
	class ComponentBase
	{
		friend class ::ComponentCollection;
	public:
		ComponentBase(ComponentKind aKind, ComponentCollection & aComponents):
			mKind(aKind),
			mComponents(aComponents)
		{
		}

		virtual ~ComponentBase() {}  // Force a virtual destructor in descendants

		/** Descendants must override to implement a specific action needed for starting the component. */
		virtual void start() = 0;

		/** Indicates that the aRequiredComponent is needed to start before this component.
		To be used only during component creation (between ComponentCollection constructor and start() call).
		Throws an Exception if called after start(). */
		inline void requireForStart(ComponentKind aRequiredComponent)
		{
			mComponents.requireForStart(mKind, aRequiredComponent);
		}


	protected:

		/** The component's kind, stored in the base so it can be queried from the base pointer. */
		ComponentKind mKind;

		/** The component collection to which this component belongs. */
		ComponentCollection & mComponents;
	};

	using ComponentBasePtr = std::shared_ptr<ComponentBase>;


public:

	/** A base class representing the common functionality in all components in the collection. */
	template <ComponentKind tKind>
	class Component:
		public ComponentBase
	{
	public:
		Component(ComponentCollection & aComponents):
			ComponentBase(tKind, aComponents)
		{
		}

		static ComponentKind kind() { return tKind; }
	};



	/** Creates a new empty collection. */
	ComponentCollection();

	/** Adds the specified component into the collection.
	Asserts that a component of the same kind doesn't already exist. */
	template <typename ComponentClass>
	void addComponent(std::shared_ptr<ComponentClass> aComponent)
	{
		addComponent(ComponentClass::kind(), aComponent);
	}


	/** Creates a new component of the specified template type,
	adds it to the collection and returns a shared ptr to it.
	Asserts that a component of the same kind doesn't already exist. */
	template <typename ComponentClass, typename... Args>
	std::shared_ptr<ComponentClass> addNew(Args &&... aArgs)
	{
		auto res = std::make_shared<ComponentClass>(*this, std::forward<Args>(aArgs)...);
		if (res == nullptr)
		{
			throw RuntimeError("Failed to create component %1", ComponentClass::kind());
		}
		addComponent(ComponentClass::kind(), res);
		return res;
	}


	/** Returns the component of the specified class.
	Usage: auto db = m_Components.get<Database>(); */
	template <typename ComponentClass>
	std::shared_ptr<ComponentClass> get()
	{
		return std::dynamic_pointer_cast<ComponentClass>(get(ComponentClass::kind()));
	}

	/** Starts all the components, in a topological order. */
	void start();

	/** Returns a logger for the specified subsystem name. */
	Logger & logger(const QString & aName);

	/** Logs directly to the specified logger. */
	template <typename... T>
	void log(const QString & aLoggerName, const QString & aFormatString, const T &... aArgs)
	{
		logger(aLoggerName).log(aFormatString, aArgs...);
	}


protected:

	/** The collection of all components. */
	std::map<ComponentKind, ComponentBasePtr> mComponents;

	/** The requirements for starting the individual components.
	Map of ComponentKind (to be started) -> vector of ComponentKind (needs to be already running). */
	std::map<ComponentKind, std::vector<ComponentKind>> mStartRequirements;

	/** Indicates whether start() has been called. */
	bool mIsStarted;


	/** Adds the specified component into the collection.
	Asserts that a component of the same kind doesn't already exist.
	Client code should use the templated version, this is its actual implementation. */
	void addComponent(ComponentKind aKind, ComponentBasePtr aComponent);

	/** Returns the component of the specified kind, as a base pointer.
	Clients should use the templated get() instead (which calls this internally). */
	ComponentBasePtr get(ComponentKind aKind);

	/** Requests that the aRequiredComponent be started before aThisComponent.
	To be used only during component creation (between ComponentCollection constructor and start() call).
	Throws an Exception if called after start(). */
	void requireForStart(ComponentKind aThisComponent, ComponentKind aRequiredComponent);

	/** Returns the mComponents in the order in which they should be started.
	Throws a LogicError if the start order cannot be constructed (due to cycles). */
	std::vector<ComponentBasePtr> componentsInStartOrder();
};
