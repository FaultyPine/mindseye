#pragma once

struct meSerializeResult
{
	enum ResultType
	{
		SER_FAILURE,
		SER_VERSION_MISMATCH,
		SER_SUCCESS,
	};
	ResultType result = SER_FAILURE;
	u32 serializedUniqueIdentifier = 0;

	meSerializeResult() = default;
	meSerializeResult(ResultType type) : result(type)
	{}
	operator ResultType() const { return result; }
	operator bool() const { return result == SER_SUCCESS; }
};

meSerializeResult SerializeFromFile(
	StringView file,
	meAllocator* allocator,
	const meTypeDescriptor& typeDescriptor,
	meSpan outBuffer);

meSerializeResult SerializeToTextBlocking(
	const meTypeDescriptor& typeDesc, 
	void* data,
	meAllocator* allocator,
    StringView& outResult);

// allocator - for dynamic allocations needed during deserialization (I.E. strings)
// outBuffer - preallocated buffer to deserialize into for POD data of the structure
meSerializeResult DeserializeFromTextBlocking(
	const meTypeDescriptor& typeDesc,
	meAllocator* allocator,
	StringView inText,
	meSpan outBuffer);


// each invocation of this func on a given string
// returns one "element" of a "list" of elements
// all elements are surrounded by an openDelim on the left 
// and a closeDelim on the right and separated by separator
// It returns each element and modifies the input string like an iterator,
// keeping track of where in the list of elements we are
// Useful when there is an unknown number of elements in a "string list"
MEAPI StringView meDeserializeEatUntilNextElement(
	StringView& str,
	char openDelim,
	char closeDelim,
	char separator);


StringView sizedBufferSerializer(const meTypeDescriptor&, SerializeContext ctx);
bool sizedBufferDeserializer(const meTypeDescriptor&, DeserializeContext& ctx);
bool stringDeserializer(const meTypeDescriptor&, DeserializeContext& ctx);

