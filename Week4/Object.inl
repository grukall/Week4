
template <typename T>
inline void RegisterClassProperties(FClassInfo* InClass)
{
	// RegisterProperties는 static 멤버라 자식 클래스에도 그대로 상속된다.
	// 그냥 호출하면 부모의 프로퍼티가 자식 FClassInfo에도 중복 등록되므로,
	// REFLECT_START가 남긴 PropertyOwnerClass로 "자기가 선언한 것"인지 확인한다.
	if constexpr (requires { typename T::PropertyOwnerClass; })
	{
		if constexpr (std::is_same_v<typename T::PropertyOwnerClass, T>)
		{
			T::RegisterProperties(InClass);
		}
	}
}

#define REFLECT_CLASS(className, superClassName)									\
public:																				\
	using ThisClass = className;													\
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
		static const bool bPropertiesRegistered = []()								\
		{																			\
			RegisterClassProperties<className>(&classInstance);						\
			return true;															\
		}();																		\
		(void)bPropertiesRegistered;												\
		return &classInstance;														\
	}																				\
virtual const FClassInfo* GetRuntimeClass() const override							\
    {                                                                               \
        return ThisClass::GetClass();                                               \
    }																				\
private:																			

// Property Reflection
#define REFLECT_START(className)										\
public:																	\
	using PropertyOwnerClass = ThisClass;								\
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
