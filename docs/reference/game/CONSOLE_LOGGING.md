# Console logging {#xengine_console_logging}

XEngine ofrece tres niveles de registro accesibles desde cualquier script que
incluya `<XEngine.h>`. Los mensajes se imprimen por consola, sin abrir ventanas
de error que interrumpan el juego.

```cpp
Console.Log("Jugador creado", "Player");
Console.Warning("Queda poca vida", "PlayerHealth");
Console.Error("No se pudo abrir el guardado", "SaveSystem");
```

El segundo argumento es el título del registro. Es opcional y toma el valor
`"Game"` si no se indica. Los títulos llevan el nivel y un icono; el texto se
muestra debajo, indentado cuatro espacios. Los mensajes multilínea conservan
la indentación de cada línea.

```text
● [LOG] Player
    Jugador creado
⚠ [WARNING] PlayerHealth
    Queda poca vida
✖ [ERROR] SaveSystem
    No se pudo abrir el guardado
```

Cuando la salida está conectada a un terminal compatible, LOG se pinta en
cian, WARNING en amarillo y ERROR en rojo. La salida redirigida a un archivo
no contiene códigos de color. Los iconos y textos se emiten en UTF-8.

Mientras XEngine está iniciado, las llamadas tradicionales del juego a
`printf`, `puts` y `std::cout` se capturan como mensajes LOG con título
`Game`. Los `printf` pendientes se vacían al procesar cada frame y al cerrar
el motor. La salida de error estándar (`stderr`) no se redirige.

La interfaz de compatibilidad `LogError(caption, error)` sigue disponible:
el primer argumento se utiliza como título y el segundo como mensaje ERROR.
`LogWarning(caption, warning)` registra advertencias del motor. Ambas se
muestran por consola sin cuadros de diálogo. El registro ERROR respeta la
configuración `Engine::logErrors`.

Las funciones `Debug::Log`, `Debug::LogWarning` y `Debug::LogError` realizan
la misma operación si se necesita acceder directamente a la API interna.
