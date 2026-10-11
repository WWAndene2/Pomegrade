package me.magnum.melonds.impl

import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.emptyFlow
import me.magnum.melonds.domain.model.DownloadProgress
import me.magnum.melonds.domain.model.appupdate.AppUpdate
import me.magnum.melonds.domain.services.UpdateInstallManager

class NoUpdateInstallManager : UpdateInstallManager {
    override fun downloadAndInstallUpdate(update: AppUpdate): Flow<DownloadProgress> {
        return emptyFlow()
    }
}