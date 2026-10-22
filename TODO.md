# TODO

1. Cabecera Estándar GBA (crt0.s & Header Checksum)

    Prioridad: Media / Baja (Fase posterior al puerto PC).

    Descripción: El ejecutable ARM producido por make gba utiliza actualmente el crt0 genérico de newlib, careciendo de la cabecera comercial de Nintendo en los primeros 192 bytes (0x08000000).

    Consecuencia: La ROM arranca en la capa de simulación nativa de PC (make pc), pero en hardware real de GBA o emuladores estrictos (como mGBA) la BIOS se congela al no validar la firma del logo de Nintendo ni la suma de comprobación del encabezado (header checksum).

    Acciones pendientes:

        Diseñar un script de enlace (linker script) y un punto de entrada en ensamblador crt0.s propio.

        Incrustar los 156 bytes del mapa de bits comprimido del Logo de Nintendo en la dirección base 0x08000004.

        Configurar el código de juego (ANJE), título interno (INITIAL D GBA) y número de revisión.

        Agregar una regla en el Makefile para calcular y parchear automáticamente la suma de comprobación (Header Checksum en el byte 0x080000BD) tras generar el binario .gba.