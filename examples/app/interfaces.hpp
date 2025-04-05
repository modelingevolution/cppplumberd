// include/ielement_info.h
#pragma once

#include <string>
#include <vector>
#include <memory>

namespace app {

    class IElementInfo;

    class IPropertyInfo {
    public:
        virtual ~IPropertyInfo() = default;

        virtual std::string GetName() const = 0;
        // Get property type
        
        virtual bool IsReadable() const = 0;
        virtual bool IsWritable() const = 0;
        virtual std::shared_ptr<int> GetValue() const = 0;
        virtual void SetValue(const int& value) = 0;
        virtual IElementInfo* GetElementInfo() const = 0;
    };

    class IElementInfo {
    public:
        virtual ~IElementInfo() = default;

        virtual std::string GetName() const = 0;

        virtual std::vector<std::shared_ptr<IPropertyInfo>> GetProperties() const = 0;
        virtual std::shared_ptr<IPropertyInfo> GetProperty(const std::string& name) const = 0;
        virtual bool HasProperty(const std::string& name) const = 0;
    };
    class IElementRegistry {
    public:
        virtual ~IElementRegistry() = default;

        virtual std::shared_ptr<IElementInfo> GetElement(
            const std::string& element_name) = 0;


        virtual std::vector<std::shared_ptr<IElementInfo>> GetAllElements() = 0;
    };
}