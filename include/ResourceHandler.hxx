#pragma once

#include <Optional.hxx>

/*
 * Simple class, mimicking the behaviour of a unique_ptr, generalizing it to allow new resources to be handled.
 * No shared resources however, because of the fact that the implementation is actually quite involved.
 * We don't support construction or assignement by handle to assure unicity, at least in a way (possible to break it with get() method).
 * The interface is thus a bit less flexible, but a bit more fool proof too. Maybe the missing features will be added later.
 * Here, we use optional, because our handle can be anything, from a simple integer to a more complexe structure. However, I do not want to force
 * the user to define a "null handle", as it can be quite complicated, unnatural, or plain impossible.
 * There is also no dereference operators, as handles do not directly point to any data in many API, and can be used to access data in multiple ways.
 *
 * The ressource management strategy must be implemented in a similar fashion as shown below :
 * class MyStrategy
 * {
 * public:
 *     using HandleType = ...;
 *     static HandleType construct(...) noexcept { ... }
 *     // WARNING : If the optional is disengaged, the destroy should do nothing, and by all mean not throwing !!!
 *     static void destroy(optional<HandleType>) noexcept { ... }
 * };
 */

template<class ResourceManagementStrategy>
class ResourceHandler
{
public:
	using RawHandleType = typename ResourceManagementStrategy::HandleType;
	using ConstructorType = decltype(ResourceManagementStrategy::construct);
	using DestructorType = decltype(ResourceManagementStrategy::destroy);

public:

	ResourceHandler() = default;

	ResourceHandler(ResourceHandler&& other) noexcept
	{
		swap(other);
		other.release();
	}

	ResourceHandler(RawHandleType handle) noexcept
	{
		internalHandle_ = handle;
	}

	ResourceHandler(const ResourceHandler&) = delete;

	~ResourceHandler() noexcept
	{
		reset({});
	}

	ResourceHandler& operator=(ResourceHandler&& other) noexcept
	{
		swap(other);
		other.release();

		return *this;
	}

	ResourceHandler& operator=(optional<RawHandleType> handle) noexcept
	{
		reset(handle);

		return *this;
	}

	ResourceHandler& operator=(const ResourceHandler&) noexcept = delete;


	optional<RawHandleType> release() noexcept
	{
		auto oldHandle = internalHandle_;

		internalHandle_ = nullopt;

		return oldHandle;
	}

	template<class ... Args>
	static ResourceHandler create(Args&&... args)
	{
		return { getConstructor()(std::forward<Args>(args)...) };
	}

	void reset(optional<RawHandleType> handle) noexcept
	{
		if(internalHandle_)
		{
			getDestructor()(internalHandle_);
		}
		internalHandle_ = handle;
	}

	optional<RawHandleType> get() const noexcept
	{
		return internalHandle_;
	}

	static ConstructorType* getConstructor() noexcept
	{
		return ResourceManagementStrategy::construct;
	}

	static DestructorType* getDestructor() noexcept
	{
		return ResourceManagementStrategy::destroy;
	}

	RawHandleType* operator->() noexcept
	{
		return &internalHandle_;
	}

	void swap(ResourceHandler& other) noexcept
	{
		using std::swap;

		swap(internalHandle_, other.internalHandle_);
	}

	operator bool() const noexcept
	{
		return internalHandle_;
	}

	template<class T>
	friend bool operator<(ResourceHandler<T>& lhs, ResourceHandler<T>& rhs);

	template<class T>
	friend bool operator>(ResourceHandler<T>& lhs, ResourceHandler<T>& rhs);

	template<class T>
	friend bool operator<=(ResourceHandler<T>& lhs, ResourceHandler<T>& rhs);

	template<class T>
	friend bool operator>=(ResourceHandler<T>& lhs, ResourceHandler<T>& rhs);

	template<class T>
	friend bool operator==(ResourceHandler<T>& lhs, ResourceHandler<T>& rhs);

	template<class T>
	friend bool operator!=(ResourceHandler<T>& lhs, ResourceHandler<T>& rhs);


private:
	optional<RawHandleType> internalHandle_;
};

/* Explicitly defaultabled comparison operators ? Pretty please C++ !
 * Here I'm not bothering defining comparison in terms of other. It's more simple that way for such
 * simple comparisons operators.
 */

template<class T>
void swap(ResourceHandler<T>& lhs, ResourceHandler<T>& rhs)
{
	lhs.swap(rhs);
}

template<class T>
bool operator<(const ResourceHandler<T>& lhs, const ResourceHandler<T>& rhs)
{
	return lhs.internalHandle_ < rhs.internalHandle_;
}

template<class T>
bool operator>(const ResourceHandler<T>& lhs, const ResourceHandler<T>& rhs)
{
	return lhs.internalHandle_ > rhs.internalHandle_;
}

template<class T>
bool operator<=(const ResourceHandler<T>& lhs, const ResourceHandler<T>& rhs)
{
	return lhs.internalHandle_ <= rhs.internalHandle_;
}

template<class T>
bool operator>=(const ResourceHandler<T>& lhs, const ResourceHandler<T>& rhs)
{
	return lhs.internalHandle_ >= rhs.internalHandle_;
}

template<class T>
bool operator==(const ResourceHandler<T>& lhs, const ResourceHandler<T>& rhs)
{
	return lhs.internalHandle_ == rhs.internalHandle_;
}

template<class T>
bool operator!=(const ResourceHandler<T>& lhs, const ResourceHandler<T>& rhs)
{
	return lhs.internalHandle_ != rhs.internalHandle_;
}
