#include "ComponentCollection.hpp"
#include <cassert>
#include <set>
#include "MultiLogger.hpp"





////////////////////////////////////////////////////////////////////////////////
// ComponentCollection:

ComponentCollection::ComponentCollection():
	mIsStarted(false)
{
}





void ComponentCollection::start()
{
	assert(!mIsStarted);
	log("main", "Starting all components...");
	auto order = componentsInStartOrder();
	for (const auto & component: order)
	{
		component->start();
	}
	mIsStarted = true;
	log("main", "All components started.");
}





Logger & ComponentCollection::logger(const QString & aName)
{
	auto multiLogger = get<MultiLogger>();
	if (!multiLogger)
	{
		throw RuntimeError("There's no MultiLogger instance in this component collection");
	}
	return multiLogger->logger(aName);
}





void ComponentCollection::addComponent(
	ComponentCollection::ComponentKind aKind,
	ComponentCollection::ComponentBasePtr aComponent
)
{
	auto itr = mComponents.find(aKind);
	if (itr != mComponents.end())
	{
		assert(!"Component of this kind is already present in the collection");
		throw LogicError(logger("main"), "Duplicate component in collection: %1", aKind);
	}
	mComponents[aKind] = aComponent;
}





ComponentCollection::ComponentBasePtr ComponentCollection::get(ComponentCollection::ComponentKind aKind)
{
	auto itr = mComponents.find(aKind);
	if (itr != mComponents.end())
	{
		return itr->second;
	}
	assert(!"Component not present in the collection");
	return nullptr;
}




void ComponentCollection::requireForStart(ComponentCollection::ComponentKind aThisComponent, ComponentCollection::ComponentKind aRequiredComponent)
{
	assert(!mIsStarted);
	assert(aThisComponent != aRequiredComponent);
	if (mIsStarted)
	{
		throw LogicError(logger("main"), "The ComponentCollection is already started.");
	}
	mStartRequirements[aThisComponent].push_back(aRequiredComponent);
}





std::vector<ComponentCollection::ComponentBasePtr> ComponentCollection::componentsInStartOrder()
{
	std::vector<ComponentCollection::ComponentBasePtr> res;
	std::set<ComponentCollection::ComponentKind> kinds;
	while (res.size() < mComponents.size())
	{
		bool hasAdded = false;
		for (const auto & componentPair: mComponents)
		{
			auto kind = componentPair.first;
			// If the component is already present in res, skip it:
			if (kinds.find(kind) != kinds.end())
			{
				continue;
			}

			// If the component has all its requirements met, add it:
			auto required = mStartRequirements[kind];
			bool canAdd = true;
			for (const auto req: required)
			{
				if (kinds.find(req) == kinds.end())
				{
					canAdd = false;
					break;
				}
			}
			if (!canAdd)
			{
				continue;
			}

			// Add the component:
			res.push_back(componentPair.second);
			kinds.insert(kind);
			hasAdded = true;
		}
		if (!hasAdded)
		{
			assert(!"Failed to calculate component start order");
			throw LogicError(logger("main"), "Failed to calculate component start order");
		}
	}
	return res;
}
