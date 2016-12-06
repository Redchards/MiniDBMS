#ifndef DB_ENTRY_HXX
#define DB_ENTRY_HXX

#include <Schema.hxx>
#include <Configuration.hxx>

#include <vector>

namespace {

template<Endianness endian, PageType type>
class DbEntryBase;

template<Endianness endian>
class DbEntryBase<endian, PageType::ReadOnly>
{
public:
	template<class T>
	DbEntryBase(const std::vector<T>& data)
	: storage_(data.begin(), data.end())
	{
		static_assert(std::is_convertible<T, char>::value || std::is_convertible<T, unsigned char>::value,
					 "The data type must be convertible to a type with byte size.");
	}

protected:
	const std::vector<uint8_t> storage_;
};

template<Endianness endian>
class DbEntryBase<endian, PageType::Writable>
{
public:
	template<class T>
	DbEntryBase(std::vector<T>& data)
	: storage_(data.begin(), data.end())
	{
		static_assert(std::is_convertible<T, char>::value || std::is_convertible<T, unsigned char>::value,
					 "The data type must be convertible to a type with byte size.");
	}

protected:
	std::vector<uint8_t> storage_;
};


}

/* WARNING : The DbSchema must be persistent, should it be destroyed at on point, the DbEntry would become unusable */
template<Endianness endian, PageType type>
class DbEntry : public DbEntryBase<endian, type>
{
private:
	template<class T, class = void>
	struct Getter;

	template<class T, class = void>
	struct Setter;

public:
	/*DbEntry(const DbSchema& schema) 
	: schema_{schema}
	{
		storage_.reserve(schema.getSize());
	}*/

	template<class T>
	DbEntry(const DbSchema& schema, const std::vector<T>& data) 
	: schema_{schema},
	  DbEntryBase<endian, type>{data}
	{}

	template<class T>
	DbEntry(const DbSchema& schema, std::vector<T>& data) 
	: schema_{schema},
	  DbEntryBase<endian, type>{data}
	{}

	std::string toString() const noexcept
	{
		std::string result;
		auto it = this->storage_.begin();
		
		for(size_type i = 0; i < schema_.getFieldCount(); ++i)
		{
			auto fieldDescriptor = schema_[i];
			auto typeDescriptor = fieldDescriptor.type;
			result += fieldDescriptor.name + " : ";
			result += Utils::RawDataStringizer<endian>::stringize(it, it + typeDescriptor.getSize(), typeDescriptor.getType());
			result += '\n';
			it += typeDescriptor.getSize();
		}	

		return result;
	}

	template<class T>
	T getAs(const std::string& fieldName)
	{
		auto index = schema_.findIndexOf(fieldName);
		if(index) return getAs(schema_.findIndexOf(fieldName));

		return {};
	}

	template<class T>
	T getAs(size_type index)
	{
		return Getter<T>::get(*this, index);
	}

	// TODO : Add type check and row existence check
	template<class T>
	void setAs(size_type index, T value)
	{
		Setter<T>::set(this->storage_, schema_, index, value);
	}

	template<class T>
	void setAs(const std::string& fieldName, T value)
	{
		size_type index = schema_.findIndexOf(fieldName);
		Setter<T>::set(this->storage_, schema_, index, value);
	}

	std::vector<uint8_t>& getRawData() noexcept
	{
		return this->storage_;
	}

	const std::vector<uint8_t>& getRawData() const noexcept
	{
		return this->storage_;
	}

	const DbSchema& getSchema() const noexcept
	{
		return schema_;
	}

	private:

	const DbSchema& schema_;
};

// TODO : Add assert to check the type.

template<Endianness endian, PageType type>
template<class T>
struct DbEntry<endian, type>::Getter<T, Meta::void_t<std::enable_if_t<std::is_integral<T>::value>>>
{
	static T get(const DbEntry<endian, type>& entry, size_type index)
	{
		auto field = entry.getSchema()[index];
		size_type fieldOffset = entry.getSchema().getFieldOffset(index);

		return Utils::RawDataConverter<endian>::rawDataToInteger(entry.getRawData().begin() + fieldOffset, entry.getRawData().begin() + fieldOffset + field.type.getSize());
	}
};

template<Endianness endian, PageType type>
template<class Dummy>
struct DbEntry<endian, type>::Getter<float, Dummy>
{
	static float get(const DbEntry<endian, type>& entry, size_type index)
	{
		auto field = entry.getSchema()[index];
		size_type fieldOffset = entry.getSchema().getFieldOffset(index);	

		return Utils::RawDataConverter<endian>::rawDataToFloat(entry.getRawData().begin() + fieldOffset, entry.getawData().begin() +  fieldOffset + field.type.getSize());
	}
};

template<Endianness endian, PageType type>
template<class Dummy>
struct DbEntry<endian, type>::Getter<double, Dummy>
{
	static double get(const DbEntry<endian, type>& entry, size_type index)
	{
		auto field = entry.getSchema()[index];
		size_type fieldOffset = entry.getSchema().getFieldOffset(index);	

		return Utils::RawDataConverter<endian>::rawDataToDouble(entry.getRawData().begin() + fieldOffset, entry.getRawData().begin() + fieldOffset + field.type.getSize());
	}
};

template<Endianness endian, PageType type>
template<class Dummy>
struct DbEntry<endian, type>::Getter<std::string, Dummy>
{
	static std::string get(const DbEntry<endian, type>& entry, size_type index)
	{
		auto field = entry.getSchema()[index];
		size_type fieldOffset = entry.getSchema().getFieldOffset(index);

		return std::string{entry.getRawData().begin() + fieldOffset, entry.getRawData().begin() + fieldOffset + field.type.getSize()};
	}
};

template<Endianness endian, PageType type>
template<class T, class Dummy>
struct DbEntry<endian, type>::Setter
{
	static void set(std::vector<uint8_t>& storage, const DbSchema& schema, size_type index, std::string value)
	{
		Utils::RawDataAdaptator<T, sizeof(T), endian> adapt{value};
		size_type offset = schema.getFieldOffset(index);
		std::copy(adapt.bytes.begin(), adapt.bytes.end(), storage.begin() + offset);
	}
};


template<Endianness endian, PageType type>
template<class Dummy>
struct DbEntry<endian, type>::Setter<std::string, Dummy>
{
	static void set(std::vector<uint8_t>& storage, const DbSchema& schema, size_type index, std::string value)
	{
		size_type offset = schema.getFieldOffset(index);
		std::copy(value.data(), value.data() + value.size(), storage.begin() + offset);

		size_type size = schema[index].type.getSize();
		if(value.size() <= size)
		{
			size_type difference = size - value.size();
			auto offsetIt = storage.begin() + offset + value.size();
			std::fill(offsetIt, offsetIt + difference, 0);
		}
	}
};

#endif // DB_ENTRY_HXX
