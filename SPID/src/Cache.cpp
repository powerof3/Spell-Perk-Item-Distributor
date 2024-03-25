#include "Cache.h"

bool FormType::GetWhitelisted(const RE::FormType a_type)
{
	return std::ranges::find(whitelist, a_type) != whitelist.end();
}
