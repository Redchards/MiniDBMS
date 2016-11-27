#ifndef DISK_PAGE_HXX
#define DISK_PAGE_HXX

#include <RawDataUtils.hxx>
#include <Optional.hxx>

#include <gsl/gsl_assert.h>

#include <string>
#include <vector>

/* Header format :
 * nextPageOffset (sizeof(std::streampos) bytes)
 * pageSize (sizeof(size_type) bytes)
 * schemaName (variant)
 * freeSlotCount (sizeof(size_type) bytes)
 * frameIndicators (pageSize bytes);
 * 
 * This utilitarian class is loading everything but the frameIndicators, which are only
 * interesting if we effectively load the page inside the main memory.
 * This class is mainly used to check if a page is full before even loading it fully into the main memory.
 */
template<Endianess endian>
class DiskPageHeader
{
	public:
	DiskPageHeader(const std::vector<uint8_t>& data)
	{
		auto it = data.begin();

		nextPageOffset_ = Utils::RawDataConverter<endian>::rawDataToStreampos(it, it + sizeof(decltype(nextPageOffset_)));
		it += sizeof(decltype(nextPageOffset_));

		pageSize_ = Utils::RawDataConverter<endian>::rawDataToInteger(it, it + sizeof(decltype(pageSize_)));
		it += sizeof(decltype(pageSize_));

		schemaName_ = std::string{it, std::find(it, data.end(), '\0')};
		it += schemaName_.size() + 1;

		freeSlotCount_ = Utils::RawDataConverter<endian>::rawDataToInteger(it, it + sizeof(decltype(freeSlotCount_)));
	}

	DiskPageHeader(const DiskPageHeader& other)
	: pageSize_{other.pageSize_},
	  nextPageOffset_{other.nextPageOffset_},
	  schemaName_{other.schemaName_},
	  freeSlotCount_{other.freeSlotCount_}
	{}

	size_type getPageSize() const noexcept
	{
		return pageSize_;
	}
	  
	size_type getNextPageOffset() const noexcept
	{
		return nextPageOffset_;
	}
	
	const std::string& getSchemaName() const noexcept
	{
		return schemaName_;
	}

	size_type getFreeSlotCount() const noexcept
	{
		return freeSlotCount_;
	}

	void decreaseFreeSlotCount(size_type val) const noexcept
	{
		Ensures(freeSlotCount_ >= val);

		freeSlotCount_ -= val;
	}

	void increaseFreeSlotCount(size_type val) const noexcept
	{
		Ensures((freeSlotCount_ + val) <= pageSize_);

		freeSlotCount_ += val;
	}

	void decrementFreeSlotCount() const noexcept
	{
		decreaseFreeSlotCount(1);
	}

	void incrementFreeSlotCount() const noexcept
	{
		incrementFreeSlotCount(1);
	}

	bool isFull() const noexcept
	{
		return (freeSlotCount_ == 0);
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

	size_type pageSize_;
	std::streampos nextPageOffset_;
	std::string schemaName_;
	size_type freeSlotCount_;
};

template<Endianess endian>
class DiskPage
{
	using PageIndex = size_type;

	public:

	/* For data, we assume that the DbSystem gave us the full page, no more, no less */
	DiskPage(PageIndex index, const std::vector<uint8_t>& data) 
	: header_{data},
	  index_{index},
	  dirtyFlag_{false}
	{
		auto it = data.begin() + sizeof(DiskPageHeader<endian>);

		frameIndicators_ = std::vector<bool>{it, it + header_.getPageSize()};
		it += header_.getPageSize();

		data_ = std::vector<uint8_t>{it, data.end()};
	}

	size_type getPageSize() const noexcept
	{
		return header_.getPageSize();
	}
	
	size_type getNextPageOffset() const noexcept
	{
		return header_.getNextPageOffset();
	}

	PageIndex getIndex() const noexcept
	{
		return index_;
	}

	size_type getFreeSlotCoutn() const noexcept
	{
		return header_.getFreeSlotCount();
	}

	const std::string& getSchemaName() const noexcept
	{
		return header_.getSchemaName();
	}
	
	bool isDirty() const noexcept
	{
		return dirtyFlag_;
	}

	bool isFull() const noexcept
	{
		return header_.isFull();
	}

	const std::vector<uint8_t>& getData() const noexcept
	{
		return data_;
	}

	void remove(size_type index) const noexcept
	{
		markDirty();
		frameIndicators_[index] = true;
		header_.incrementFreeSlotCount();
	}

	bool isFree(size_type index) const noexcept
	{
		return !frameIndicators_[index];
	}

	bool add(const DbEntry<endian>& entry) const noexcept
	{
		auto freeIndex = findFreeIndex();

		if(freeIndex)
		{
			replace(*freeIndex, entry);
			markDirty();
			frameIndicators_[*freeIndex] = false;
			header_.decrementFreeSlotCount();

			return true;
		}

		return false;
	}

	void replace(size_type index, const DbEntry<endian>& entry) const noexcept
	{
		Ensures(entry.getSchema().getName() == getSchemaName());

		auto rawData = entry.getRawData();
		std::move(rawData.begin(), rawData.end(), data_.begin() + (index * entry.getSize));
		markDirty();
	}

	private:

	optional<size_type> findFreeIndex() const noexcept
	{
		for(size_type i = 0; i < frameIndicators_.size(); ++i)
		{
			if(isFree(i)) return i;
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
