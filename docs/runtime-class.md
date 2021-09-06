# Runtime class

Panda runtime uses `panda::Class` to store all necessary language independent information about classes. Virtual table and region for static fields are embedded to the `panda::Class` object so it has variable size. To get fast access to them, the `Class Word` field of the object header points to the instance of this class. `ClassLinker::GetClass` also returns an instance of the `panda::Class`.

Pointer to the managed class object (instance of `panda.Class` or `java.lang.Class` in case of Java, for example) can be obtained using `panda::Class::GetManagedObject` method:

```cpp
panda::Class *cls = obj->ClassAddr()->GetManagedObject();
```

Storing common runtime information separately from managed objects allows more flexibility for the layout but causes additional dereferences to get panda::Class from mirror classes and vice versa. However, we can use composition to reduce the number of additional dereferences.

```cpp
namespace panda::coretypes {
class Class : public ObjectHeader {

    ... // Mirror fields

    panda::Class klass_;
};
}  // namespace panda::coretypes
```

The layout of the `coretypes::Class` is as follows:


    mirror class (`coretypes::Class`) --------> +------------------+ <-+
                                                |    `Mark Word`   |   |
                                                |    `Class Word`  |-----+
                                                +------------------+   | |
                                                |   Mirror fields  |   | |
        panda class (`panda::Class`) ---------> +------------------+ <-|-+
                                                |      ...         |   |
                                                | `Managed Object` |---+
                                                |      ...         |
                                                +------------------+

Note: The `panda::Class` object must be the last in the mirror class because it has variable size.

Such layout allows to get pointer to the `panda::Class` object from the `coretypes::Class` one and vice versa without dereferencies if we know language context and it's constant (some language specific code):

```cpp
auto *managed_class_obj = coretypes::Class::FromRuntimeClass(klass);
...
auto *runtime_class = managed_class_obj->GetRuntimeClass();
```

Where `coretypes::Class::FromRuntimeClass` and `coretypes::Class::GetRuntimeClass` are implemented in the following way:


```cpp
namespace panda::coretypes {
class Class : public ObjectHeader {
    ...

    panda::Class *GetRuntimeClass() {
        return &klass_;
    }

    static constexpr size_t GetRuntimeClassOffset() {
        return MEMBER_OFFSET(Class, klass_);
    }

    static Class *FromRuntimeClass(panda::Class *klass) {
        return reinterpret_cast<Class *>(reinterpret_cast<uintptr_t>(klass) - GetRuntimeClassOffset());
    }

    ...
};
}  // namespace panda::coretypes
```

In common places where language context can be different we can use `panda::Class::GetManagedObject`. For example:

```cpp
auto *managed_class_obj = klass->GetManagedObject();
ObjectLock lock(managed_class_obj);
```

Instead of

```cpp
ObjectHeader *managed_class_obj;
switch (klass->GetSourceLang()) {
    case PANDA_ASSEMBLY: {
        managed_class_obj = coretypes::Class::FromRuntimeClass(klass);
        break;
    }
    case JAVA_8: {
        managed_class_obj = java::JClass::FromRuntimeClass(klass);
        break;
    }
    ...
}
ObjectLock lock(managed_class_obj);
```
