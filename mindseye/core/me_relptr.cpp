#include "me_relptr.h"

struct meRelPtrTestRecord
{
	meRelPtr ptr = {};
	int value = 0;
};

void meRelPtrTests()
{
	meRelPtr defaultPtr = {};
	ME_ASSERT(!defaultPtr);
	ME_ASSERT(defaultPtr.Get<int>() == nullptr);

	meRelPtrTestRecord record = {};
	record.value = 42;
	record.ptr = &record.value;
	ME_ASSERT(record.ptr);
	ME_ASSERT(record.ptr.Get<int>() == &record.value);
	ME_ASSERT(*record.ptr.Get<int>() == 42);
	ME_ASSERT(*record.ptr.operator-><int>() == 42);
	ME_ASSERT(record.ptr.operator*<int>() == 42);

	const meRelPtrTestRecord& constRecord = record;
	ME_ASSERT(constRecord.ptr);
	ME_ASSERT(constRecord.ptr.Get<int>() == &constRecord.value);
	ME_ASSERT(*constRecord.ptr.Get<int>() == 42);
	ME_ASSERT(*constRecord.ptr.operator-><int>() == 42);
	ME_ASSERT(constRecord.ptr.operator*<int>() == 42);

	meRelPtrTestRecord copy = record;
	ME_ASSERT(copy.ptr);
	ME_ASSERT(copy.ptr.Get<int>() == &copy.value);
	ME_ASSERT(*copy.ptr.Get<int>() == 42);

	copy.value = 321;
	ME_ASSERT(*record.ptr.Get<int>() == 42);
	ME_ASSERT(*copy.ptr.Get<int>() == 321);

	record.ptr.Clear();
	ME_ASSERT(!record.ptr);
	ME_ASSERT(record.ptr.Get<int>() == nullptr);
}

