#ifndef DB_SYSTEM_HXX
#define DB_SYSTEM_HXX

#include <Configuration.hxx>
#include <DbSchema.hxx>
#include <FileValueReader.hxx>
#include <Optional.hxx>

#include <string>
#include <unordered_map>
#include <vector>

template<Endianess endian>
class DbSystem
{
	public:
	DbSystem(std::string dbFile, std::string schemaFile)
	: dbFile_{dbFile},
	  schemaFile_{schemaFile}
	{
		FileValueReader<endian> schemaReader{schemaFile};

		while(!schemaReader.eof())
		{
			size_type serialDataSize = reader.readValue(8);

			std::vector<uint8_t> serialData{serialDataSize};
			reader.read(serialData, serialDataSize);

			schemaList_.push_back(DbSchemaSerializer<endian>::deserialize(serialData));
			schemaMapping_[schemaList_.back().getName()] = schemaList_.size() - 1;
		}
	}

	optional<DbSchema> getSchema(size_t index) const noexcept
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

	private:

	std::vector<DbSchema> schemaList_;
	std::unordered_map<std::string, size_type> schemaMapping_;

	std::string dbFile_;
	std::string schemaFile_;
};

#endif // DB_SYSTEM_HXX
