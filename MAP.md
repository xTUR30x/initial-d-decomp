[ 1. ENTRY POINT (ROM Hardware / C Native Main) ]
                        │
                        ▼
            0x080000C0 / main()
                        │
         ┌──────────────┴──────────────┐
         ▼                             ▼
   [ BIOS / HW INIT ]         [ SDL2 / PC INIT ]
   • Limpieza VRAM/IWRAM      • Ventana y Renderizador
   • Config. Timers / DMA     • Audio Subsystem (SDL_Audio)
   • Modo Vídeo Inicial       • Estructuras de Memoria Base
         │                             │
         └──────────────┬──────────────┘
                        │
                        ▼
[ 2. BUCLE PRINCIPAL DE EJECUCIÓN (Main Loop & Engine Core) ]
                        │
                        ├──────────────────────────┐
                        ▼                          ▼
             input_read_keys()           interrupt_vblank_handler()
             • Convierte Teclado/Mando   • DMA Palette Transfer (0x08028AC4)
             • Mapea a REG_KEYINPUT      • Motor de Fundidos (0x08028D60)
                        │               • Actualiza Puntero Fade (0x030044D8)
                        └─────────────┬────────────┘
                                      │
                                      ▼
             [ 3. STATE DISPATCHER / JUMP TABLE (0x080002F8) ]
                                      │
              Calcula índice: g_game_state_table[current_state]()
                                      │
     ┌────────────────────────────────┼────────────────────────────────┐
     │ (Slot 0)                       │ (Slot 1)                       │ (Slot 2...43)
     ▼                                ▼                                ▼
state_00_intro()             state_01_title_menu()             state_02_mode_select()
(0x080A52E4)                 (0x080A39F4 / 0x080A3EB0)         (0x0809D34C)
• Pantallas de Logo          • Bucle visual del Menú           • Selección de modo
• Carga Paleta Inicial       • Lee Confirmación / START        • Transiciones
• Carga Música Intro         • Pide cambio a State 23          • Submenús