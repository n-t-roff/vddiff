#include "ver.h"

/* pre-alpha: Developmemt phase, implementation, feature changes.
 *            Major changes in this phase may cause malfunction of many
 *            features.  This may not be detected before alpha test phase.
 *
 *     |      All planned features are implemented, changes are done,
 *     v      TODO list is empty.
 *
 * alpha:     Test phase.  All features are tested.  This may result in
 *            new feature requirements (or significant changes) and hence
 *            a transition back to pre-alpha phase.
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

const char version[] = "version 1.3.0-pre-alpha  "
    "2016-12-10 22:22"
    ;
