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
		std::cout << "Blouh" << std::endl;
		FileValueReader<endian> schemaReader{schemaFile};
		while(!schemaReader.eof())
		{
			size_type serialDataSize = schemaReader.readValue(8);

			std::vector<uint8_t> serialData(serialDataSize);
			schemaReader.read(serialData, serialDataSize);

			schemaList_.push_back(DbSchemaSerializer<endian>::deserialize(serialData));
			schemaMapping_[schemaList_.back().getName()] = schemaList_.size() - 1;
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

	private:

	void addNewPage(const DbEntry<endian>& entry)
	{

		DiskPage<endian> newPage{0, entry.getSchema(), pageSize_};
		newPage.add(entry);
		PageWriter<endian> pgWriter{dbFile_};
		std::streamoff newOffset = pgWriter.getFileSize();

		auto lastPageOffset = lastOffsetMap_.find(entry.getSchema().getName());

		if(lastPageOffset != lastOffsetMap_.end())
		{
			PageReader<endian> pgReader{dbFile_};
			DiskPageHeader<endian> lastPageHeader = pgReader.readPageHeader(lastPageOffset->second);
			lastPageHeader.setNextPageOffset(newOffset);

			pgWriter.writePageHeader(lastPageHeader, lastPageOffset->second);
		}

		newPage.add(entry);
		pgWriter.appendPage(newPage);
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
