/**
 * KeyListViewModel 单元测试
 *
 * 测试钥匙列表页面的 ViewModel 业务逻辑。
 * 使用 MockK 模拟 KeyRepository，验证 ViewModel 的状态流转。
 */
package com.digitalkey.app.key

import com.digitalkey.app.data.model.KeyModel
import com.digitalkey.app.data.model.KeyStatus
import com.digitalkey.app.data.model.Permission
import com.digitalkey.app.data.model.PermissionType
import com.digitalkey.app.data.model.UiState
import com.digitalkey.app.data.repository.KeyRepository
import io.mockk.coEvery
import io.mockk.coVerify
import io.mockk.impl.annotations.RelaxedMockK
import io.mockk.junit4.MockKRule
import io.mockk.mockk
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.StandardTestDispatcher
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.runTest
import kotlinx.coroutines.test.setMain
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test

@OptIn(ExperimentalCoroutinesApi::class)
class KeyListViewModelTest {

    @get:Rule
    val mockkRule = MockKRule(this)

    @RelaxedMockK
    private lateinit var keyRepository: KeyRepository

    private lateinit var viewModel: KeyListViewModel
    private val testDispatcher = StandardTestDispatcher()
    private val testScope = TestScope(testDispatcher)

    private val mockKeys = listOf(
        KeyModel(
            id = "key-001",
            name = "我的主钥匙",
            vehicleId = "v001",
            vehicleName = "特斯拉 Model 3",
            vehiclePlate = "京A·12345",
            permissions = listOf(
                Permission(PermissionType.UNLOCK, true),
                Permission(PermissionType.LOCK, true),
                Permission(PermissionType.START_ENGINE, true)
            ),
            status = KeyStatus.ACTIVE,
            issuerId = "admin",
            issuerName = "管理员",
            issuedAt = "2026-05-01T10:00:00Z",
            createdAt = "2026-05-01T10:00:00Z",
            updatedAt = "2026-05-06T08:00:00Z"
        ),
        KeyModel(
            id = "key-002",
            name = "家人备用钥匙",
            vehicleId = "v001",
            vehicleName = "特斯拉 Model 3",
            vehiclePlate = "京A·12345",
            permissions = listOf(
                Permission(PermissionType.UNLOCK, true),
                Permission(PermissionType.LOCK, true)
            ),
            status = KeyStatus.ACTIVE,
            issuerId = "admin",
            issuerName = "管理员",
            issuedAt = "2026-05-02T14:00:00Z",
            expiresAt = "2026-12-31T23:59:59Z",
            createdAt = "2026-05-02T14:00:00Z",
            updatedAt = "2026-05-02T14:00:00Z"
        )
    )

    @Before
    fun setUp() {
        Dispatchers.setMain(testDispatcher)
        // mock 默认返回成功
        coEvery { keyRepository.getKeys() } returns Result.success(mockKeys)
        coEvery { keyRepository.deleteKey(any()) } returns Result.success(Unit)
        coEvery { keyRepository.updateKeyName(any(), any()) } returns Result.success(mockKeys.first())
    }

    @After
    fun tearDown() {
        Dispatchers.resetMain()
    }

    @Test
    fun `init should load keys and emit Success state`() = runTest(testDispatcher) {
        viewModel = KeyListViewModel(keyRepository)

        testDispatcher.scheduler.advanceUntilIdle()

        val state = viewModel.keyListState.value
        assert(state is UiState.Success) { "Initial state should be Success" }
        val successState = state as UiState.Success
        assert(successState.data.size == 2) { "Should have 2 keys" }
    }

    @Test
    fun `loadKeys should emit Loading then Success`() = runTest(testDispatcher) {
        viewModel = KeyListViewModel(keyRepository)
        testDispatcher.scheduler.advanceUntilIdle()

        viewModel.loadKeys()
        testDispatcher.scheduler.advanceUntilIdle()

        val state = viewModel.keyListState.value
        assert(state is UiState.Success) { "State should be Success after loadKeys()" }
    }

    @Test
    fun `loadKeys should emit Error when repository fails`() = runTest(testDispatcher) {
        coEvery { keyRepository.getKeys() } returns Result.failure(RuntimeException("网络错误"))

        viewModel = KeyListViewModel(keyRepository)
        testDispatcher.scheduler.advanceUntilIdle()

        val state = viewModel.keyListState.value
        assert(state is UiState.Error) { "State should be Error on failure" }
        val errorState = state as UiState.Error
        assert(errorState.message.contains("网络错误")) { "Error message should match" }
    }

    @Test
    fun `loadKeys should emit Empty when no keys`() = runTest(testDispatcher) {
        coEvery { keyRepository.getKeys() } returns Result.success(emptyList())

        viewModel = KeyListViewModel(keyRepository)
        testDispatcher.scheduler.advanceUntilIdle()

        val state = viewModel.keyListState.value
        assert(state is UiState.Empty) { "State should be Empty when no keys" }
    }

    @Test
    fun `selectKey should update selectedKey state`() = runTest(testDispatcher) {
        viewModel = KeyListViewModel(keyRepository)
        testDispatcher.scheduler.advanceUntilIdle()

        viewModel.selectKey(mockKeys[1])

        assert(viewModel.selectedKey.value == mockKeys[1]) { "Selected key should match" }
    }

    @Test
    fun `clearSelection should reset selectedKey to null`() = runTest(testDispatcher) {
        viewModel = KeyListViewModel(keyRepository)
        testDispatcher.scheduler.advanceUntilIdle()

        viewModel.selectKey(mockKeys[0])
        assert(viewModel.selectedKey.value != null) { "Key should be selected" }

        viewModel.clearSelection()
        assert(viewModel.selectedKey.value == null) { "Selection should be cleared" }
    }

    @Test
    fun `refreshKeys should reload and return Success`() = runTest(testDispatcher) {
        viewModel = KeyListViewModel(keyRepository)
        testDispatcher.scheduler.advanceUntilIdle()

        viewModel.refreshKeys()
        testDispatcher.scheduler.advanceUntilIdle()

        val state = viewModel.keyListState.value
        assert(state is UiState.Success) { "State should be Success after refresh" }
        assert(!viewModel.isRefreshing.value) { "isRefreshing should be false after refresh" }
    }

    @Test
    fun `deleteKey should call repository and reload`() = runTest(testDispatcher) {
        viewModel = KeyListViewModel(keyRepository)
        testDispatcher.scheduler.advanceUntilIdle()

        var callbackResult = false
        viewModel.deleteKey("key-001") { callbackResult = it }
        testDispatcher.scheduler.advanceUntilIdle()

        coVerify(exactly = 1) { keyRepository.deleteKey("key-001") }
        coVerify(atLeast = 2) { keyRepository.getKeys() } // init + reload after delete
        assert(callbackResult) { "Delete callback should return true" }
    }

    @Test
    fun `deleteKey should propagate failure to callback`() = runTest(testDispatcher) {
        coEvery { keyRepository.deleteKey(any()) } returns Result.failure(RuntimeException("删除失败"))
        viewModel = KeyListViewModel(keyRepository)
        testDispatcher.scheduler.advanceUntilIdle()

        var callbackResult = true
        viewModel.deleteKey("key-001") { callbackResult = it }
        testDispatcher.scheduler.advanceUntilIdle()

        assert(!callbackResult) { "Delete callback should return false on failure" }
    }

    @Test
    fun `updateKeyName should call repository and reload`() = runTest(testDispatcher) {
        viewModel = KeyListViewModel(keyRepository)
        testDispatcher.scheduler.advanceUntilIdle()

        var callbackResult = false
        viewModel.updateKeyName("key-001", "新名称") { callbackResult = it }
        testDispatcher.scheduler.advanceUntilIdle()

        coVerify(exactly = 1) { keyRepository.updateKeyName("key-001", "新名称") }
        assert(callbackResult) { "Update callback should return true" }
    }

    @Test
    fun `updateKeyName should propagate failure to callback`() = runTest(testDispatcher) {
        coEvery { keyRepository.updateKeyName(any(), any()) } returns Result.failure(RuntimeException("更新失败"))
        viewModel = KeyListViewModel(keyRepository)
        testDispatcher.scheduler.advanceUntilIdle()

        var callbackResult = true
        viewModel.updateKeyName("key-001", "新名称") { callbackResult = it }
        testDispatcher.scheduler.advanceUntilIdle()

        assert(!callbackResult) { "Update callback should return false on failure" }
    }
}
