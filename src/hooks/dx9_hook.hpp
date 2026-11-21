#pragma once

namespace waw::hooks {

bool InstallDx9Hook();
void ShutdownDx9Hook();
bool IsDx9ReadyToUnload();

} // namespace waw::hooks
