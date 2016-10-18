#ifndef INC_OGUIFORMATTEDCOMMANDIMPL_H
#define INC_OGUIFORMATTEDCOMMANDIMPL_H

#include "IOguiFormattedCommand.h"

#include <map>

class OguiFormattedCommandImpl : public IOguiFormattedCommand
{
public:

    OguiFormattedCommandImpl() { }
    virtual ~OguiFormattedCommandImpl() { }

    virtual void execute(OguiFormattedText* text, const std::string& parameters, OguiFormattedText::ParseData* data) = 0;

protected:
    virtual void parseParameters(const std::string& params);

    const std::string& castParameter(const std::string& name, const std::string& default_value)
    {
        if (parameters.find(name) == parameters.end())
            return default_value;

        return parameters[name];
    }

    int castParameter(const std::string& name, int default_value)
    {
        if (parameters.find(name) == parameters.end())
            return default_value;

        return std::stoi(parameters[name]);
    }

    bool castParameter(const std::string& name, bool default_value)
    {
        if (parameters.find(name) == parameters.end())
            return default_value;

        return std::stoi(parameters[name]) != 0;
    }

    std::map< std::string, std::string >	parameters;
};

#endif
