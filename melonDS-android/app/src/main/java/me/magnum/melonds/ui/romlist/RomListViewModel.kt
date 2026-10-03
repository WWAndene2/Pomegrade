package me.magnum.melonds.ui.romlist

import android.net.Uri
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.flow.launchIn
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import me.magnum.melonds.common.DirectoryAccessValidator
import me.magnum.melonds.common.Permission
import me.magnum.melonds.common.UriPermissionManager
import me.magnum.melonds.domain.model.RomScanningStatus
import me.magnum.melonds.domain.model.SortingMode
import me.magnum.melonds.domain.model.SortingOrder
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.domain.model.rom.RomPlatform
import me.magnum.melonds.domain.repositories.RomsRepository
import me.magnum.melonds.domain.repositories.SettingsRepository
import me.magnum.melonds.impl.PomegradeFolder
import me.magnum.melonds.impl.RomIconProvider
import me.magnum.melonds.utils.EventSharedFlow
import me.magnum.melonds.utils.SubjectSharedFlow
import java.text.Normalizer
import java.util.Calendar
import javax.inject.Inject

@HiltViewModel
class RomListViewModel @Inject constructor(
    private val romsRepository: RomsRepository,
    private val settingsRepository: SettingsRepository,
    private val romIconProvider: RomIconProvider,
    private val uriPermissionManager: UriPermissionManager,
    private val directoryAccessValidator: DirectoryAccessValidator,
    private val pomegradeFolder: PomegradeFolder,
) : ViewModel() {

    // games found outside the Pomegrade folder are moved into it after each scan
    private val _organizing = MutableStateFlow(false)
    val organizing = _organizing.asStateFlow()
    private val _organizeResult = EventSharedFlow<PomegradeFolder.OrganizeResult>()
    val organizeResult: Flow<PomegradeFolder.OrganizeResult> = _organizeResult

    private val _searchQuery = MutableStateFlow("")
    // Pomegrade: the console shown, null = every console (remembered)
    private val _platformFilter = MutableStateFlow(settingsRepository.getRomPlatformFilter())
    val platformFilter = _platformFilter.asStateFlow()
    private val _sortingMode = MutableStateFlow(settingsRepository.getRomSortingMode())
    private val _sortingOrder = MutableStateFlow(settingsRepository.getRomSortingOrder())

    private val _hasSearchDirectories = SubjectSharedFlow<Boolean>()
    val hasSearchDirectories: Flow<Boolean> = _hasSearchDirectories

    private val _invalidDirectoryAccessEvent = EventSharedFlow<Unit>()
    val invalidDirectoryAccessEvent: Flow<Unit> = _invalidDirectoryAccessEvent

    private val _roms = MutableStateFlow<List<Rom>?>(null)
    val roms = _roms.asStateFlow()

    val onRomIconFilteringChanged = settingsRepository.observeRomIconFiltering()

    val romScanningStatus = romsRepository.getRomScanningStatus()

    init {
        viewModelScope.launch {
            // after a scan, not during one (both would change the ROM list)
            var scanned = false
            romScanningStatus.collect { status ->
                if (status == RomScanningStatus.SCANNING) {
                    scanned = true
                } else if (scanned) {
                    scanned = false
                    organizeNewGames()
                }
            }
        }

        viewModelScope.launch {
            settingsRepository.observeRomSearchDirectories()
                .distinctUntilChanged()
                .collect { directories ->
                    _hasSearchDirectories.tryEmit(directories.isNotEmpty())
                }
        }

        combine(romsRepository.getRoms(), _searchQuery, _platformFilter) { allRoms, query, platform ->
            val roms = if (platform == null) allRoms else allRoms.filter { it.platform == platform }
            val romList = if (query.isEmpty()) {
                roms
            } else {
                withContext(Dispatchers.Default) {
                    roms.filter { rom ->
                        if (!isActive) {
                            return@withContext emptyList()
                        }

                        val normalizedName = Normalizer.normalize(rom.name, Normalizer.Form.NFD).replace("[^\\p{ASCII}]", "")
                        val normalizedPath = Normalizer.normalize(rom.fileName, Normalizer.Form.NFD).replace("[^\\p{ASCII}]", "")

                        normalizedName.contains(query, true) || normalizedPath.contains(query, true)
                    }
                }
            }

            _roms.value = when (_sortingMode.value) {
                SortingMode.ALPHABETICALLY -> romList.sortedWith(buildAlphabeticalRomComparator(_sortingOrder.value))
                SortingMode.RECENTLY_PLAYED -> romList.sortedWith(buildRecentlyPlayedRomComparator(_sortingOrder.value))
            }
        }.launchIn(viewModelScope)

        combine(_sortingMode, _sortingOrder) { sortingMode, sortingOrder ->
            _roms.value = when (sortingMode) {
                SortingMode.ALPHABETICALLY -> _roms.value?.sortedWith(buildAlphabeticalRomComparator(sortingOrder))
                SortingMode.RECENTLY_PLAYED -> _roms.value?.sortedWith(buildRecentlyPlayedRomComparator(sortingOrder))
            }
        }.launchIn(viewModelScope)
    }

    fun refreshRoms() {
        romsRepository.rescanRoms()
    }

    fun setRomLastPlayedNow(rom: Rom) {
        romsRepository.setRomLastPlayed(rom, Calendar.getInstance().time)
    }

    fun setPlatformFilter(platform: RomPlatform?) {
        settingsRepository.setRomPlatformFilter(platform)
        _platformFilter.value = platform
    }

    fun setRomSearchQuery(query: String?) {
        _searchQuery.tryEmit(Normalizer.normalize(query ?: "", Normalizer.Form.NFD).replace("[^\\p{ASCII}]", ""))
    }

    fun setRomSorting(sortingMode: SortingMode) {
        if (sortingMode == _sortingMode.value) {
            val newSortingOrder = if (_sortingOrder.value == SortingOrder.ASCENDING)
                SortingOrder.DESCENDING
            else
                SortingOrder.ASCENDING

            settingsRepository.setRomSortingOrder(_sortingOrder.value)
            _sortingOrder.value = newSortingOrder
        } else {
            settingsRepository.setRomSortingMode(sortingMode)
            settingsRepository.setRomSortingOrder(sortingMode.defaultOrder)

            _sortingMode.value = sortingMode
            _sortingOrder.value = sortingMode.defaultOrder
        }
    }

    private suspend fun organizeNewGames() {
        if (_organizing.value || !pomegradeFolder.canOrganize()) {
            return
        }
        val roms = romsRepository.getRoms().first()
        if (pomegradeFolder.gamesToOrganize(roms).isEmpty()) {
            return
        }
        _organizing.value = true
        val result = withContext(Dispatchers.IO) {
            pomegradeFolder.organize(roms) { }
        }
        _organizing.value = false
        _organizeResult.tryEmit(result)
    }

    fun addRomSearchDirectory(directoryUri: Uri) {
        val accessValidationResult = directoryAccessValidator.getDirectoryAccessForPermission(directoryUri, Permission.READ_WRITE)

        if (accessValidationResult == DirectoryAccessValidator.DirectoryAccessResult.OK) {
            uriPermissionManager.persistDirectoryPermissions(directoryUri, Permission.READ_WRITE)
            // with the Pomegrade folder set up, a folder is added beside its Roms (whose games it
            // will move there); otherwise it replaces the ROM folder, as upstream
            if (pomegradeFolder.isSetUp()) {
                settingsRepository.includeRomSearchDirectory(directoryUri)
            } else {
                settingsRepository.addRomSearchDirectory(directoryUri)
            }
        } else {
            _invalidDirectoryAccessEvent.tryEmit(Unit)
        }
    }

    suspend fun getRomIcon(rom: Rom): RomIcon {
        val romIconBitmap = romIconProvider.getRomIcon(rom)
        val iconFiltering = settingsRepository.getRomIconFiltering()
        return RomIcon(romIconBitmap, iconFiltering)
    }

    private fun buildAlphabeticalRomComparator(sortingOrder: SortingOrder): Comparator<Rom> {
        return if (sortingOrder == SortingOrder.ASCENDING) {
            Comparator { o1: Rom, o2: Rom ->
                o1.name.compareTo(o2.name)
            }
        } else {
            Comparator { o1: Rom, o2: Rom ->
                o2.name.compareTo(o1.name)
            }
        }
    }

    private fun buildRecentlyPlayedRomComparator(sortingOrder: SortingOrder): Comparator<Rom> {
        return if (sortingOrder == SortingOrder.ASCENDING) {
            Comparator { o1: Rom, o2: Rom ->
                when {
                    o1.lastPlayed == null -> -1
                    o2.lastPlayed == null -> 1
                    else -> o1.lastPlayed!!.compareTo(o2.lastPlayed)
                }
            }
        } else {
            Comparator { o1: Rom, o2: Rom ->
                when {
                    o2.lastPlayed == null -> -1
                    o1.lastPlayed == null -> 1
                    else -> o2.lastPlayed!!.compareTo(o1.lastPlayed)
                }
            }
        }
    }
}