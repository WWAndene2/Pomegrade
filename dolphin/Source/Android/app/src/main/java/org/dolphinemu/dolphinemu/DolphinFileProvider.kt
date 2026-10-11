// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu

import androidx.core.content.FileProvider

/**
 * Pomegrade: Dolphin's FileProvider under a class of its own. In one app, each provider element needs its
 * own class: the Switch core declares androidx's FileProvider too, with another authority.
 */
class DolphinFileProvider : FileProvider()
