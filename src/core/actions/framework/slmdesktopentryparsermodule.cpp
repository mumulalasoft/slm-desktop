#include "slmdesktopentryparsermodule.h"

namespace Slm::Actions::Framework {

DesktopParseResult DesktopEntryParserModule::parseFile(const QString &path) const
{
    return m_parser.parseFile(path);
}

} // namespace Slm::Actions::Framework

