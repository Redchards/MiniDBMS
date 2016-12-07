#ifndef SCHEMA_HXX
#define SCHEMA_HXX

#include <string>
#include <unordered_map>
#include <vector>

#include <Configuration.hxx>
#include <DataTypes.hxx>
#include <Optional.hxx>

struct FieldDescriptor
{
	FieldDescriptor(const std::string& name_, DataTypeDescriptor type_)
	: name{name_},
	  type{type_}
	{}
	
	std::string name;
	DataTypeDescriptor type;	
};

class DbSchema 
{
	public:
	DbSchema(std::string name, const std::vector<FieldDescriptor>& internal) 
	: name_{name}, 
	  internal_{internal},
	  size_{computeSize()},
	  dataSize_{computeDataSize()}
	{}
	
	DbSchema(std::string name, std::vector<FieldDescriptor>&& internal) 
	: name_{name}, 
	  internal_{std::move(internal)},
	  size_{computeSize()},
	  dataSize_{computeDataSize()}
	{}

	DbSchema(const DbSchema&) = default;
	DbSchema(DbSchema&&) = default;

	
	const std::string& getName() const noexcept
	{
		return name_;
	}

	private:

	size_type computeSize() const noexcept
	{
		size_type space = name_.length() + 1;
		for(auto& elem : internal_)
		{

	std::cout << "LLLL ? " << space << std::endl;	
			space += elem.name.length() + 1 + DataTypeDescriptor::getPackedSize();
		std::cout << "LLLL ... " << DataTypeDescriptor::getPackedSize() << " : " << elem.name.size() << std::endl;
		}
		
		return space;
	}

	size_type computeDataSize() const noexcept
	{
		size_type size = 0;

		for(auto& elem : internal_)
		{
			size += elem.type.getSize();
		}

		return size;
	}

	public:
	size_type getSize() const noexcept
	{
		return size_;
	}

	size_type getDataSize() const noexcept
	{
		return dataSize_;
	}
	
	size_type getFieldCount() const noexcept
	{
		return internal_.size();
	}
	
	FieldDescriptor at(size_type index) const noexcept
	{
		return internal_[index];
	}
	
	FieldDescriptor operator[](size_type index) const noexcept
	{
		return at(index);
	}
	
	optional<size_type> findIndexOf(const std::string& fieldName) const
	{
		auto it = std::find_if(internal_.begin(), internal_.end(), [&fieldName](const auto& elem) {
			return elem.name == fieldName;
		});

		if(it == internal_.end()) return {};

		return std::distance(internal_.begin(), it);
		
	}

	size_type getFieldOffset(size_type index) const noexcept
	{
		size_type offset = 0;

		for(size_type i = 0; i < index; ++i)
		{
			offset += internal_[i].type.getSize();
		}

		return offset;
	}

	size_type getFieldOffset(const std::string& fieldName) const noexcept
	{
		return getFieldOffset(findIndexOf(fieldName));
	}

	private:
	std::string name_;
	std::vector<FieldDescriptor> internal_;
	size_type size_;
	size_type dataSize_;

	static constexpr size_type nameLengthFieldSize = 2;
};

#endif // SCHEMA_HXX
