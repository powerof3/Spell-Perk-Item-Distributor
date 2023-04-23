#pragma once
#include "Configs.h"
#include "Expressions.h"

namespace Configs
{
	template <typename Input, typename Output>
	struct Decoder
	{};

	// Input is a filename of the config.
	template <typename Output>
	struct ConfigDecoder : Decoder<std::filesystem::path, Output>
	{};

	// Input is a raw string to be decoded.
	template <typename Output>
	struct ValueDecoder : Decoder<std::string, Output>
	{};

	template <typename T, typename Output>
	concept value_decoder_type = std::derived_from<T, ValueDecoder<Output>>;

	// Input is a Config describing file to be decoded.
	// Output is void. All decoded entries are appended directly to passed config object.
	struct DataConfigDecoder : ConfigDecoder<void>
	{
		void decode(Config& config);
	};

	// Input is raw string value containing filtering expression.
	// Output is an Expression.
	template <typename ExpressionTarget>
	struct ExpressionDecoder : Decoder<std::string, Expressions::Expression<ExpressionTarget>>
	{
	};
}

// INI
namespace Configs
{

	// Input is a filename of the config.
	// Specific INI config reader.
	template <typename Output> 
	struct INIConfigDecoder : ConfigDecoder<Output>
	{
		CSimpleIniA ini;

		bool load(const std::filesystem::path& path)
		{
			// TODO: Throw decoding error here.
			return ini.LoadFile(path.string().c_str()) >= 0;
		}
	};

	struct DataINIConfigDecoder : DataConfigDecoder, INIConfigDecoder<void>
	{};

	struct InvalidINIException : std::exception
	{};

}
