package me.magnum.melonds.di

import dagger.Module
import dagger.Provides
import dagger.hilt.InstallIn
import dagger.hilt.components.SingletonComponent
import me.magnum.melonds.domain.repositories.UpdatesRepository
import me.magnum.melonds.domain.services.UpdateInstallManager
import me.magnum.melonds.impl.NoUpdateInstallManager
import me.magnum.melonds.impl.NoUpdatesRepository
import javax.inject.Singleton

@Module
@InstallIn(SingletonComponent::class)
object UpdatesModule {
    @Provides
    @Singleton
    fun provideUpdatesRepository(): UpdatesRepository {
        return NoUpdatesRepository()
    }

    @Provides
    @Singleton
    fun provideUpdateInstallManager(): UpdateInstallManager {
        return NoUpdateInstallManager()
    }
}