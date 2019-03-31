#pragma once





#include <utility>
#include "Exception.hpp"





/** A wrapper that either holds a value or is empty. */
template <typename T>
class Optional
{
public:
	Optional():
		mIsPresent(false)
	{
	}


	Optional(const T & aValue):
		mIsPresent(true),
		mValue(aValue)
	{
	}


	Optional(const T && aValue):
		mIsPresent(true),
		mValue(std::move(aValue))
	{
	}



	// Copy-constructor:
	Optional(const Optional<T> & aOther):
		mIsPresent(aOther.mIsPresent),
		mValue(aOther.mValue)
	{
	}



	// Move-constructor:
	Optional(Optional<T> && aOther):
		mIsPresent(aOther.mIsPresent),
		mValue(std::move(aOther.mValue))
	{
	}



	constexpr bool operator == (const Optional<T> & aOther) const noexcept
	{
		return (
			(mIsPresent == aOther.mIsPresent) &&
			(
				!mIsPresent ||
				(mValue == aOther.mValue)
			)
		);
	}


	constexpr bool isPresent() const noexcept
	{
		return mIsPresent;
	}


	/** Returns a mutable reference to the stored value.
	Throws a LogicError when empty. */
	constexpr T & value() &
	{
		if (mIsPresent)
		{
			return mValue;
		}
		else
		{
			throw LogicError("Optional value not present");
		}
	}


	/** Returns a const reference to the stored value.
	Throws a LogicError when empty. */
	constexpr const T & value() const &
	{
		if (mIsPresent)
		{
			return mValue;
		}
		else
		{
			throw LogicError("Optional value not present");
		}
	}


	/** Returns the stored value if it is present, or the specified value if not present. */
	T valueOr(const T & aDefault) const
	{
		if (mIsPresent)
		{
			return mValue;
		}
		else
		{
			return aDefault;
		}
	}


	/** Returns the stored value if it is present, or the default-contructed value if not present. */
	T valueOrDefault() const
	{
		if (mIsPresent)
		{
			return mValue;
		}
		else
		{
			return T();
		}
	}


	/** Regular assignment operator. */
	Optional<T> & operator = (const Optional<T> & aOther)
	{
		if (&aOther == this)
		{
			return *this;
		}
		mIsPresent = aOther.mIsPresent;
		mValue = aOther.mValue;
		return *this;
	}


	/** Move-assignment operator. */
	Optional<T> & operator = (Optional<T> && aOther)
	{
		mIsPresent = std::move(aOther.mIsPresent);
		mValue = std::move(aOther.mValue);
		return *this;
	}


	/** Assigns the new value to the container */
	Optional<T> & operator = (const T & aValue)
	{
		mIsPresent = true;
		mValue = aValue;
		return *this;
	}


	/** Move-assigns the new value to the container. */
	Optional<T> & operator = (T && aValue)
	{
		mIsPresent = true;
		mValue = std::move(aValue);
		return *this;
	}


	/** Assigns the new value to the container.
	If the variant type is convertible to our type, assigns the value.
	If the variant is empty or not convertible, empties this object. */
	Optional<T> & operator = (const QVariant & aValue)
	{
		if (!aValue.isValid() || aValue.isNull())
		{
			mIsPresent = false;
		}
		else if (aValue.canConvert<T>())
		{
			mIsPresent = true;
			mValue = aValue.value<T>();
		}
		else
		{
			mIsPresent = false;
		}
		return *this;
	}


protected:

	/** Specifies whether the object is holding a valid value (false => empty). */
	bool mIsPresent;

	/** The value held by the object. Only valid if mIsPresent is true. */
	T mValue;
};
