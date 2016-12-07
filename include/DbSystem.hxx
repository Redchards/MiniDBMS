#ifndef DB_SYSTEM_HXX
#define DB_SYSTEM_HXX

#include <Configuration.hxx>
#include <Schema.hxx>
#include <FileValueReader.hxx>
#include <Optional.hxx>
#include <BufferManager.hxx>
#include <PageWriter.hxx>

#include <string>
#include <unordered_map>
#include <vector>

template<Endianness endian>
class DbSystem;

template<Endianness endian>
class DbIterator
{

friend class DbSystem<endian>;

public:
	DbIterator(BufferManager<endian>& bufferManager, const DbSchema& schema)
	: bufferManager_{bufferManager},
	  schema_{schema},
	  pageHandle_{},
	  currentEntryIndex_{0}
	{
		pageHandle_ = bufferManager_.template requestFirstPage<PageType::Writable>(schema.getName());
	}

	DbEntry<endian> operator*() noexcept
	{
		auto dataStart = pageHandle_.get()->getData().begin() + (currentEntryIndex_ * schema_.getDataSize());
		std::vector<uint8_t> tmp(dataStart, dataStart + schema_.getDataSize());
		return {schema_, tmp};
	}

	DiskPage<endian>& getPage() noexcept
	{
		return *pageHandle_.get();
	}

	size_type getCurrentIndex() const noexcept
	{
		return currentEntryIndex_;
	}

	DbIterator& operator++() 
	{
		if(pageHandle_)
		{
			do
			{
				++currentEntryIndex_;
			
				if(currentEntryIndex_ >= pageHandle_.get()->getPageSize())
				{
					pageHandle_ = bufferManager_.template requestNextPage<PageType::Writable>(*pageHandle_.get());
					currentEntryIndex_ = 0;
				}
			} while(pageHandle_ && pageHandle_.get()->isFree(currentEntryIndex_));
		}

		return *this;
	}

	bool operator==(const DbIterator<endian>& other)
	{
		return ((&bufferManager_ == &other.bufferManager_)
			&& (&schema_ == &other.schema_)
			&& (pageHandle_ == other.pageHandle_)
			&& (currentEntryIndex_ == other.currentEntryIndex_));
	}

	bool operator!=(const DbIterator<endian>& other)
	{
		return !(*this == other);
	}

private:

	// The "end" constructor, used by the DbSystem to create the end iterator
	DbIterator(BufferManager<endian>& bufferManager, const DbSchema& schema, bool)
	: bufferManager_{bufferManager},
	  schema_{schema},
	  pageHandle_{},
	  currentEntryIndex_{0}
	{}

	BufferManager<endian>& bufferManager_;
	const DbSchema& schema_;
	BufferedPageHandle<endian, PageType::Writable> pageHandle_;
	size_type currentEntryIndex_;
};

template<Endianness endian>
class DbSystem
{
	static constexpr size_type defaultPageSize = 512;

	public:
	DbSystem(std::string dbFile, std::string schemaFile, size_type pageSize = defaultPageSize)
	: dbFile_{dbFile},
	  schemaFile_{schemaFile},
	  bufferManager_{dbFile},
	  pageSize_{pageSize},
	  lastOffsetMap_{}
	{
		FileValueReader<endian> schemaReader{schemaFile};
		schemaReader.rewind();
		while(!schemaReader.eof())
		{
			size_type serialDataSize = schemaReader.readValue(8);

			std::vector<uint8_t> serialData(serialDataSize);
			schemaReader.read(serialData, serialDataSize);

			schemaList_.push_back(DbSchemaSerializer<endian>::deserialize(serialData));
			schemaMapping_[schemaList_.back().getName()] = schemaList_.size() - 1;
		}
		std::cout << schemaList_.size() << std::endl;
		for(const DbSchema& schema : schemaList_)
		{
			auto offset = bufferManager_.lookForLastPage(schema.getName());
			if(offset)
			{
				lastOffsetMap_.insert({schema.getName(), *offset});
			}
		}
	}

	~DbSystem()
	{
		FileValueWriter<endian> schemaWriter{schemaFile_};

		for(auto& schema : schemaList_)
		{
    		auto serialData = DbSchemaSerializer<endian>::serialize(schema);
			schemaWriter.write(serialData);
		}
	}

	void addSchema(const DbSchema& newSchema) noexcept
	{
		schemaList_.push_back(newSchema);
	}

	optional<const DbSchema&> getSchema(size_type index) const noexcept
	{
		if(index >= schemaList_.size())
		{
			return {};
		}
		return schemaList_.at(index);
	}

	optional<size_type> getSchemaIndex(const std::string& schemaName) const noexcept
	{
		auto it = schemaMapping_.find(schemaName);
		if(it == schemaMapping_.end())
		{
			return {};
		}
		return it->second;
	}

	void add(const DbEntry<endian>& entry)
	{
		// Make sure that the entry has a valid schema ?
		auto freePageHandle = bufferManager_.template requestFreePage<PageType::Writable>(entry.getSchema());
		if(freePageHandle.get())
		{
			freePageHandle.get()->add(entry);
		}
		else
		{
			addNewPage(entry);
		}
	}

	DbIterator<endian> getIterator(const std::string& schemaName) noexcept
	{
		auto schemaIndex = getSchemaIndex(schemaName);
		return {bufferManager_, *getSchema(*schemaIndex)};
	}

	DbIterator<endian> endIterator(const std::string& schemaName) noexcept
	{
		auto schemaIndex = getSchemaIndex(schemaName);
		return {bufferManager_, *getSchema(*schemaIndex), true}; 
	}

	template<class T>
	void updateWhen(const std::string& schemaName, const std::string& updatedField, T value, std::function<bool(DbEntry<endian>&)> pred)
	{
		auto it = getIterator(schemaName);

		while(it != endIterator(schemaName))
		{
			auto entry = *it;

			if(pred(entry))
			{
				entry.template setAs<T>(updatedField, value);
				it.getPage().replace(it.getCurrentIndex(), entry);
			}

			++it;
		}
	}

	void removeWhen(const std::string& schemaName, std::function<bool(DbEntry<endian>&)> pred)
	{
		auto it = getIterator(schemaName);

		while(it != endIterator(schemaName))
		{
			auto entry =  *it;
			std::cout << "Extra" << std::endl;
			if(pred(entry))
			{
				std::cout << entry.toString() << std::endl;
				it.getPage().remove(it.getCurrentIndex());
			}
			++it;

			std::cout << "Nul" << std::endl;
		}
	}

	private:

	void addNewPage(const DbEntry<endian>& entry)
	{

		DiskPage<endian> newPage{0, entry.getSchema(), pageSize_};
		PageWriter<endian> pgWriter{dbFile_};

		std::streamoff newOffset = pgWriter.getFileSize();

		newPage.add(entry);
		pgWriter.appendPage(newPage);

		auto lastPageOffset = lastOffsetMap_.find(entry.getSchema().getName());

		if(lastPageOffset != lastOffsetMap_.end())
		{
			auto lastPageHandle = bufferManager_.template requestPage<PageType::Writable>(lastPageOffset->second);
			lastPageHandle.get()->setNextPageOffset(newOffset);
			bufferManager_.flush(lastPageHandle.get()->getIndex());

			lastOffsetMap_[entry.getSchema().getName()] = newOffset;
		}

		lastOffsetMap_.insert({entry.getSchema().getName(), newOffset});

	}

	std::vector<DbSchema> schemaList_;
	std::unordered_map<std::string, size_type> schemaMapping_;
	BufferManager<endian> bufferManager_;
	size_type pageSize_;
	std::unordered_map<std::string, std::streamoff> lastOffsetMap_;

	std::string dbFile_;
	std::string schemaFile_;
};

#endif // DB_SYSTEM_HXX
