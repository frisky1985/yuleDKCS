/**
 * DigitalKey App - Hilt依赖注入模块
 */
package com.digitalkey.app.di

import com.digitalkey.app.data.repository.KeyRepository
import com.digitalkey.app.data.repository.VehicleRepository
import dagger.Module
import dagger.Provides
import dagger.hilt.InstallIn
import dagger.hilt.components.SingletonComponent
import javax.inject.Singleton

/**
 * App级别的Hilt模块
 */
@Module
@InstallIn(SingletonComponent::class)
object AppModule {

    @Provides
    @Singleton
    fun provideKeyRepository(): KeyRepository {
        return KeyRepository()
    }

    @Provides
    @Singleton
    fun provideVehicleRepository(): VehicleRepository {
        return VehicleRepository()
    }
}
