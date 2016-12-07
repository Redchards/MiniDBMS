#ifndef DISK_PAGE_HXX
#define DISK_PAGE_HXX

#include <RawDataUtils.hxx>
#include <Optional.hxx>
#include <Configuration.hxx>

#include <gsl/gsl_assert.h>

#include <string>
#include <vector>

/* Header format :
 * nextPageOffset (sizeof(std::streamoff) bytes)
 * rawPageSize (sizeof(size_type) bytes)
 * headerSize (sizeof(uint32_t) bytes)
 * pageSize (sizeof(size_type) bytes)
 * schemaName (variant)
 * freeSlotCount (sizeof(size_type) bytes)
 * frameIndicators (pageSize bytes);
 * 
 * This utilitarian class is loading everything but the frameIndicators, which are only
 * interesting if we effectively load the page inside the main memory.
 * This class is mainly used to check if a page is full before even loading it fully into the main memory.
 */
template<Endianness endian>
class DiskPageHeader
{
	public:
	DiskPageHeader(const std::vector<uint8_t>& data)
	{
		auto it = data.begin();

		nextPageOffset_ = Utils::RawDataConverter<endian>::rawDataToStreamoff(it, it + sizeof(decltype(nextPageOffset_)));
		it += sizeof(decltype(nextPageOffset_));

		rawPageSize_ = Utils::RawDataConverter<endian>::rawDataToInteger(it, it + sizeof(decltype(rawPageSize_)));
		it += sizeof(decltype(rawPageSize_));

		headerSize_ = Utils::RawDataConverter<endian>::rawDataToInteger(it, it + sizeof(decltype(headerSize_)));
		it += sizeof(decltype(headerSize_));

		pageSize_ = Utils::RawDataConverter<endian>::rawDataToInteger(it, it + sizeof(decltype(pageSize_)));
		it += sizeof(decltype(pageSize_));

		schemaName_ = std::string{it, std::find(it, data.end(), '\0')};
		it += schemaName_.size() + 1;

		freeSlotCount_ = Utils::RawDataConverter<endian>::rawDataToInteger(it, it + sizeof(decltype(freeSlotCount_)));
	}

	DiskPageHeader(std::streamoff nextPageOffset, size_type elemSize, size_type pageSize, const std::string& schemaName, size_type freeSlotCount)
	: nextPageOffset_{nextPageOffset},
	  rawPageSize_{},
	  headerSize_{},
	  pageSize_{pageSize},
	  schemaName_{schemaName},
	  freeSlotCount_{freeSlotCount}
	{
		rawPageSize_ = getSize() + (elemSize * pageSize) + pageSize;
		headerSize_ = getSize();
	}

	DiskPageHeader(const DiskPageHeader& other)
	: nextPageOffset_{other.nextPageOffset_},
	  pageSize_{other.pageSize_},
	  rawPageSize_{other.rawPageSize_},
	  headerSize_{other.headerSize_},
	  schemaName_{other.schemaName_},
	  freeSlotCount_{other.freeSlotCount_}
	{}

	size_type getPageSize() const noexcept
	{
		return pageSize_;
	}
	  
	std::streamoff getNextPageOffset() const noexcept
	{
		return nextPageOffset_;
	}

	void setNextPageOffset(std::streamoff offset) noexcept
	{
		nextPageOffset_ = offset;
	}

	void setNextPageOffset(std::streamoff newOffset) const noexcept
	{
		nextPageOffset_ = newOffset;
	}
	
	const std::string& getSchemaName() const noexcept
	{
		return schemaName_;
	}

	size_type getFreeSlotCount() const noexcept
	{
		return freeSlotCount_;
	}

	size_type getRawPageSize() const noexcept
	{
		return rawPageSize_;
	}

	uint32_t getHeaderSize() const noexcept
	{
		return headerSize_;
	}

	void decreaseFreeSlotCount(size_type val) noexcept
	{
		Ensures(freeSlotCount_ >= val);

		freeSlotCount_ -= val;
	}

	void increaseFreeSlotCount(size_type val) noexcept
	{
		Ensures((freeSlotCount_ + val) <= pageSize_);

		freeSlotCount_ += val;
	}

	void decrementFreeSlotCount() noexcept
	{
		decreaseFreeSlotCount(1);
	}

	void incrementFreeSlotCount() noexcept
	{
		increaseFreeSlotCount(1);
	}

	bool isFull() const noexcept
	{
		return (freeSlotCount_ == 0);
	}

	uint32_t getSize() const noexcept
	{
		return (sizeof(decltype(nextPageOffset_))
			  + sizeof(decltype(pageSize_))
			  + sizeof(decltype(rawPageSize_))
			  + sizeof(decltype(headerSize_))
			  + schemaName_.size()
			  + sizeof(decltype(freeSlotCount_)) + 1);  // Count the null terminator.
		//return (3 * sizeof(size_type)) + sizeof(uint32_t) + sizeof(std::streamoff) + schemaName_.size() + 1;
	}

	private:
	DataTypeDescriptor retrieveTypeDescriptor(const std::vector<uint8_t>& data) const noexcept
	{
		constexpr size_type offset  = sizeof(decltype(nextPageOffset_)) + sizeof(decltype(pageSize_));
		auto it = data.begin() + offset;

		auto dataTypeId = Utils::RawDataConverter<endian>::rawDataToInteger(it, it + sizeof(DataType::underlying_type));
		it += sizeof(DataType::underlying_type);

		auto modifier = Utils::RawDataConverter<endian>::rawDataToInteger(it, it + sizeof(size_type));
		it += sizeof(size_type);

		return {DataType::fromValue(dataTypeId), modifier};
	}

	std::streamoff nextPageOffset_;
	size_type pageSize_;
	size_type rawPageSize_;
	uint32_t headerSize_;
	std::string schemaName_;
	size_type freeSlotCount_;
};

template<Endianness endian>
class DiskPage
{
	public:
	using PageIndex = size_type;

	public:

	/* For data, we assume that the DbSystem gave us the full page, no more, no less */
	DiskPage(PageIndex index, const std::vector<uint8_t>& data) 
	: header_{data},
	  index_{index},
	  dirtyFlag_{false}
	{
		auto it = data.begin() + header_.getSize();
		frameIndicators_ = std::vector<bool>{it, it + header_.getPageSize()};
		it += header_.getPageSize();

		data_ = std::vector<uint8_t>{it, data.end()};
	}

	DiskPage(PageIndex index, const DbSchema& schema, size_type pageSize)
	: header_{0, schema.getDataSize(), pageSize, schema.getName(), pageSize},
	  index_{index},
	  dirtyFlag_{false},
	  frameIndicators_(pageSize, false),
	  data_(pageSize * schema.getDataSize(), 0) 
	{
	}

	size_type getPageSize() const noexcept
	{
		return header_.getPageSize();
	}
	
	std::streamoff getNextPageOffset() const noexcept
	{
		return header_.getNextPageOffset();
	}

	void setNextPageOffset(std::streamoff offset) noexcept
	{
		header_.setNextPageOffset(offset);
		markDirty();
	}

	PageIndex getIndex() const noexcept
	{
		return index_;
	}

	const DiskPageHeader<endian>& getHeader() const noexcept
	{
		return header_;
	}

	size_type getFreeSlotCount() const noexcept
	{
		return header_.getFreeSlotCount();
	}

	const std::string& getSchemaName() const noexcept
	{
		return header_.getSchemaName();
	}
	
	size_type getRawPageSize() const noexcept
	{
		return header_.getRawPageSize();
	}

	uint32_t getHeaderSize() const noexcept
	{
		return header_.getHeaderSize();
	}

	bool isDirty() const noexcept
	{
		return dirtyFlag_;
	}

	bool isFull() const noexcept
	{
		return header_.isFull();
	}

	const std::vector<bool>& getFrameIndicators() const noexcept
	{
		return frameIndicators_;
	}

	const std::vector<uint8_t>& getData() const noexcept
	{
		return data_;
	}

	void remove(size_type index) noexcept
	{
		if(frameIndicators_[index] != false)
		{
			markDirty();
			header_.incrementFreeSlotCount();
			frameIndicators_[index] = false;
		}
	}

	bool isFree(size_type index) const noexcept
	{
		return !frameIndicators_[index];
	}

	bool add(const DbEntry<endian>& entry) noexcept
	{
		auto freeIndex = findFreeIndex();
		std::cout << "add : " << getIndex() << " : " << getFreeSlotCount() << std::endl;
		if(freeIndex)
		{
			replace(*freeIndex, entry);
			markDirty();
			frameIndicators_[*freeIndex] = true;
			header_.decrementFreeSlotCount();

			return true;
		}

		return false;
	}

	void replace(size_type index, const DbEntry<endian>& entry) noexcept
	{
		Ensures(entry.getSchema().getName() == getSchemaName());

		auto rawData = entry.getRawData();
		std::copy(rawData.begin(), rawData.end(), data_.begin() + (index * rawData.size()));
		//std::cout << "SSSSS : " << data_.size()  << " : " << rawData.size() << " : " << (index * rawData.size()) << std::endl;
		/*std::cout << "DDD " << rawData.size() << std::endl;
		for(auto d : rawData)
		{ std::cout << (int)d << " ";}*/
		markDirty();
	}

	void linkTo(std::streamoff offset) noexcept
	{
		header_.setNextPageOffset(offset);
	}

	private:

	optional<size_type> findFreeIndex() const noexcept
	{
		for(size_type i = 0; i < frameIndicators_.size(); ++i)
		{
			if(isFree(i)){ return i; }
		}
		return {};
	}

	void markDirty() noexcept
	{
		dirtyFlag_ = true;
	}

	DiskPageHeader<endian> header_;
	PageIndex index_;
	bool dirtyFlag_;
	std::vector<bool> frameIndicators_;
	std::vector<uint8_t> data_;
};

#endif // DISK_PAGE_HXX
