// G2AlikeMaths - module names and verdicts, inside the library. GPLv3, see LICENSE.
#ifndef G2AM_MODULE_NAMES_H
#define G2AM_MODULE_NAMES_H

const char * g2am_type_name(int type);
int          g2am_type_from_name(const char * name);   // -1 if unknown
int          g2am_type_at(int i, int extensions);       // -1 past the end
const char * g2am_type_status(int type);

#endif
