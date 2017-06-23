#include "ver.h"

/* pre-alpha: Developmemt phase, implementation, feature changes,
 *            done on a limited set of operating systems.
 *            Major changes in this phase may cause malfunction of many
 *            features.  This may not be detected before alpha test phase.
 *
 *     |      All planned features are implemented, changes are done,
 *     v      TODO list is empty.
 *
 * alpha:     Test phase.  Tool usability and all features are tested on
 *            all target operating systems.  This may result in new
 *            feature requirements or significant changes.
 *     |
 *     v      Feature freeze
 *
 * beta:      Test phase.  All features are tested again.  For each bug
 *            it is decided if it gets fixed before or after the release.
 *     |
 *     v      Code freeze
 *
 * RC<n>:     Test phase.  All features are tested again.  If no test
 *            failes, this will become the release version. */

const char version[] =
"1.10.0-alpha "
"2017-06-23 15:32"
;
