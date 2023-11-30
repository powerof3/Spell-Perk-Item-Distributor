#include "FormData.h"

std::size_t Forms::GetTotalEntries()
{
	std::size_t size = 0;

	ForEachDistributable([&]<typename Form>(Distributables<Form>& a_distributable) {
		size += a_distributable.GetSize();
	});

	return size;
}

std::size_t Forms::GetTotalLeveledEntries()
{
	std::size_t size = 0;

	ForEachDistributable([&]<typename Form>(Distributables<Form>& a_distributable) {
		size += a_distributable.GetLeveledSize();
	});

	return size;
}
