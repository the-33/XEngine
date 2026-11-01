#pragma once
/** @file ErrorHandler.h
 * @brief Runtime diagnostic logging functions.
 */

#include <string>
#include <string_view>

#ifdef _WIN32
// Avoid including unnecessary parts of Windows.h
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// Avoid conflicts with min/max
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#endif

/** @brief Valores de compatibilidad de la antigua API de cuadros de diálogo.
 * @note El registro actual no abre ventanas: solo devuelve OK cuando la
 *       opción Engine::logErrors está desactivada o Error en otro caso.
 */
enum class ErrorLogResult
{
    OK = 1,      ///< User selected OK in the dialog.
    Cancel = 2,  ///< User cancelled the dialog.
    Abort = 3,   ///< User selected Abort.
    Retry = 4,   ///< User selected Retry.
    Ignore = 5,  ///< User selected Ignore.
    Yes = 6,     ///< User confirmed Yes.
    No = 7,      ///< User selected No.
    Close = 8,   ///< User closed the dialog via Close.
    Help = 9,    ///< User selected Help.
    TryAgain = 10, ///< User selected Try Again.
    Continue = 11, ///< User selected Continue.
    Error = -1   ///< No dialog result is available or the operation failed.
};

/** @brief Registra un error con origen y sin mostrar cuadros de diálogo.
 * @param caption Subsistema o función que ha detectado el fallo.
 * @param error Descripción del fallo.
 * @return Error si se ha registrado; OK si Engine::logErrors está desactivado.
 * @note El argumento opcional de Windows se ignora por compatibilidad.
 */
ErrorLogResult LogError(const std::string& caption,
    const std::string& error
#ifdef _WIN32
    , UINT msgBoxType = MB_OK | MB_ICONSTOP | MB_SETFOREGROUND
#endif
);

/** @brief Registra un error con el texto adicional de SDL.
 * @param caption Subsistema o función que ha detectado el fallo.
 * @param error Descripción principal.
 * @param sdlError Texto opcional devuelto por SDL_GetError(), puede ser nullptr.
 * @return Error si se ha registrado; OK si Engine::logErrors está desactivado.
 */
ErrorLogResult LogError(const std::string& caption,
    const std::string& error,
    const char* sdlError
#ifdef _WIN32
    , UINT msgBoxType = MB_OK | MB_ICONSTOP | MB_SETFOREGROUND
#endif
);

/** @brief Registro de consola con niveles y colores, similar a Unity.
 * Los mensajes incluyen iconos. Los colores se omiten al redirigir la salida
 * a archivos o procesos que no son terminales.
 * @ingroup xengine_services
 */
namespace Debug {
    /** @brief Escribe un mensaje informativo.
     * @param message Mensaje UTF-8 del juego o del motor.
     * @param title Título que identifica el origen del mensaje.
     */
    void Log(std::string_view message, std::string_view title = "Game") noexcept;
    /** @brief Escribe una advertencia sin bloquear la ejecución.
     * @param message Mensaje UTF-8 de advertencia.
     * @param title Título que identifica el origen de la advertencia.
     */
    void LogWarning(std::string_view message, std::string_view title = "Game") noexcept;
    /** @brief Escribe un error respetando Engine::logErrors.
     * @param message Mensaje UTF-8 del error.
     * @param title Título que identifica el origen del error.
     */
    void LogError(std::string_view message, std::string_view title = "Game") noexcept;
}

/** @brief Acceso directo al registro de consola desde los scripts del juego.
 * @code{.cpp}
 * Console.Log("Jugador creado");
 * Console.Warning("Quedan pocas vidas");
 * Console.Error("No se pudo cargar la textura");
 * @endcode
 * @ingroup xengine_services
 */
struct ConsoleLogger {
    /** @brief Escribe una línea LOG en la consola.
     * @param message Mensaje UTF-8 que se quiere registrar.
     * @param title Título opcional del registro.
     */
    void Log(std::string_view message, std::string_view title = "Game") const noexcept {
        Debug::Log(message, title);
    }
    /** @brief Escribe una línea WARNING en la consola.
     * @param message Mensaje UTF-8 de advertencia.
     * @param title Título opcional del registro.
     */
    void Warning(std::string_view message, std::string_view title = "Game") const noexcept {
        Debug::LogWarning(message, title);
    }
    /** @brief Escribe una línea ERROR en la consola.
     * @param message Mensaje UTF-8 del error.
     * @param title Título opcional del registro.
     */
    void Error(std::string_view message, std::string_view title = "Game") const noexcept {
        Debug::LogError(message, title);
    }
};

/** @brief Global console logger; no initialization is required. */
inline constexpr ConsoleLogger Console{};

/** @brief Compatibility facade for existing games; prefer Console.Log/Warning/Error.
 * @deprecated Migrate to the PascalCase Console API.
 */
struct LegacyConsoleLogger {
    /** @brief Legacy alias of Console.Log.
     * @param message Message to record.
     * @param title Diagnostic category.
     */
    void log(std::string_view message, std::string_view title = "Game") const noexcept { Console.Log(message, title); }
    /** @brief Legacy alias of Console.Warning.
     * @param message Warning description.
     * @param title Diagnostic category.
     */
    void warning(std::string_view message, std::string_view title = "Game") const noexcept { Console.Warning(message, title); }
    /** @brief Legacy alias of Console.Error.
     * @param message Error description.
     * @param title Diagnostic category.
     */
    void error(std::string_view message, std::string_view title = "Game") const noexcept { Console.Error(message, title); }
};
/** @brief Legacy lower-case facade retained for source compatibility. */
[[deprecated("Use Console.Log(), Console.Warning() or Console.Error()")]]
inline constexpr LegacyConsoleLogger console{};

/** @brief Registra una advertencia con la función de origen.
 * @param caption Subsistema que detectó la condición.
 * @param warning Descripción de la advertencia.
 */
void LogWarning(const std::string& caption, const std::string& warning) noexcept;

/** @brief Captura stdout durante la ejecución del motor.
 * Las llamadas del juego a printf, puts o std::cout pasan a Debug::Log.
 * @ingroup xengine_services
 */
namespace XEngineConsole {
    /** @brief Inicia la captura; si falla, stdout conserva su comportamiento.
     * @return True cuando stdout queda redirigido al registro.
     */
    bool BeginStdoutCapture() noexcept;
    /** @brief Vacía el búfer y restaura el descriptor de stdout original. */
    void EndStdoutCapture() noexcept;
}
