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
			std::cout << "Heree" << std::endl;
			auto offset = bufferManager_.lookForLastPage(schema.getName());
			std::cout << "Heree" << std::endl;
			if(offset)
			{
				std::cout << "add offset" << std::endl;
				lastOffsetMap_.insert({schema.getName(), *offset});
			}
		}
	}

	optional<DbSchema&> getSchema(size_t index) const noexcept
	{
		if(index >= schemaList_.size())
		{
			return {};
		}
		return schemaList_.at(index);
	}

	optional<size_type> getSchemaIndex(std::string schemaName) const noexcept
	{
		auto it = schemaMapping_.find(schemaName);
		if(it == schemaMapping_.end())
		{
			return {};
		}
		return *it;
	}

	template<PageType type>
	void add(const DbEntry<endian, type>& entry)
	{
		// Make sure that the entry has a valid schema ?
		auto freePageHandle = bufferManager_.template requestFreePage<PageType::Writable>(entry.getSchema());
		if(freePageHandle.get())
		{
			freePageHandle.get()->add(entry);
		}
		else
		{
			std::cout << "Add new " << std::endl;
			addNewPage(entry);
		}
	}

	private:

	void addNewPage(const DbEntry<endian, PageType::ReadOnly>& entry)
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
