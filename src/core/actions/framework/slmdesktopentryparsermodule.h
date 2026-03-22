#pragma once

#include "../slmdesktopactionparser.h"

namespace Slm::Actions::Framework {

class DesktopEntryParserModule
{
public:
    DesktopParseResult parseFile(const QString &path) const;

private:
    DesktopActionParser m_parser;
};

} // namespace Slm::Actions::Framework

