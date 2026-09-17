
#define REFLECT_CLASS(className, superClassName)									\
public:																				\
	using Super = superClassName;													\
	static const FClassInfo* GetClass()												\
	{																				\
		static FClassInfo classInstance = FClassInfo(								\
			#className,																\
			superClassName::GetClass(),												\
			[]() -> UObject* {														\
				if constexpr (std::is_abstract_v<className>)						\
				{																	\
					return nullptr;													\
				}																	\
				else {																\
					return new className();											\
				}																	\
			}																		\
		);																			\
		return &classInstance;														\
	}																				\
private:

// Property Reflection
#define REFLECT_START(className)										\
public:																	\
	inline static void RegisterProperties(FClassInfo* InClass)				\
	{

#define PROPERTY(PropertyName)											\
    InClass->AddProperty<decltype(ThisClass::PropertyName)>(#PropertyName, offsetof(ThisClass, PropertyName));

#define REFLECT_END()													\
	};																	\
private:


template<typename TObject>
	requires std::derived_from<TObject, UObject>
bool UObject::IsA() const
{
	return IsA(TObject::GetClass());
}

template<typename TObject>
	requires std::derived_from<TObject, UObject>
TObject* UObject::Cast()
{
	if (IsA<TObject>())
	{
		return static_cast<TObject*>(this);
	}
	return nullptr;
}

template<typename TObject>
	requires std::derived_from<TObject, UObject>
TObject* UObject::GetObjectByUUID(int32 uuid)
{
	UObject* object = GetObjectByUUID(uuid);
	if (object && object->IsA<TObject>())
	{
		return static_cast<TObject*>(object);
	}
	return nullptr;
}

template<typename TObject>
	requires std::derived_from<TObject, UObject>
TObject* UObject::GetObjectByInternalIndex(uint32 internalIndex)
{
	UObject* object = GetObjectByInternalIndex(internalIndex);
	if (object && object->IsA<TObject>())
	{
		return static_cast<TObject*>(object);
	}
	return nullptr;
}
