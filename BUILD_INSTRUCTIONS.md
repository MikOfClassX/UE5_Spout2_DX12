# Build Instructions

Guida rapida per compilare il plugin `Spout2_DX12` da riga di comando e
installarlo in un progetto Unreal Engine 5.

---

## 1. Requisiti

- Windows con **Unreal Engine 5.8** (default `C:\Program Files\Epic Games\UE_5.8`).
- Visual Studio 2022, workload **"Game development with C++"**.
- GPU/driver DirectX 12 (solo a runtime, non serve per compilare).

Non serve CMake: il plugin si compila con `RunUAT.bat BuildPlugin`, lo
stesso comando usato da Epic per generare i binari del Marketplace.

---

## 2. Compilare

```bat
Build.bat [EngineDir] [OutputDir]
```

| Argomento   | Default                               |
|-------------|----------------------------------------|
| `EngineDir` | `C:\Program Files\Epic Games\UE_5.8`   |
| `OutputDir` | `.\Build\Spout2_DX12`                  |

```bat
Build.bat
Build.bat "" "E:\PluginOutput\Spout2_DX12"
Build.bat "D:\Epic Games\UE_5.8" "E:\PluginOutput\Spout2_DX12"
```


## 3. Cosa produce

```
Build\Spout2_DX12\
├── Spout2_DX12.uplugin
├── Binaries\Win64\
│   ├── UnrealEditor-Spout2_DX12.dll   <- modulo compilato (Editor)
│   ├── UnrealEditor-Spout2_DX12.pdb
│   ├── UnrealEditor.modules
│   ├── Spout.dll                      <- runtime Spout
│   └── SpoutDX12.dll                  <- runtime Spout
├── Source\
│   ├── Spout2_DX12\                   <- sorgenti del modulo plugin (Public/Private + .Build.cs)
│   └── ThirdParty\                    <- SDK Spout, incluso come sorgente
│       ├── include\                   <- header Spout (usati tutti, SpoutDX12.h li include a cascata)
│       ├── lib\Win64\SpoutDX12.lib    <- unica .lib effettivamente linkata
│       └── bin\Win64\                 <- Spout.dll + SpoutDX12.dll, uniche DLL caricate a runtime
├── Resources\Icon128.png
└── Intermediate\                      <- cache di build, non serve distribuirla
```

> `Source\ThirdParty\` contiene anche altri file (cartella `Spout2_DX12Library\`,
> le altre `.lib`/`.dll` come `SpoutDX`, `SpoutDX9`, `SpoutLibrary`) che sono
> varianti dell'SDK Spout non referenziate da `Spout2_DX12.Build.cs`: non
> servono a compilare né a far girare il plugin, sono solo eredità del setup
> iniziale.

`Source\` è incluso interamente così l'Editor può ricompilare il plugin per
altre versioni dell'Engine senza dipendenze esterne. Tutto è distribuibile
tranne `Intermediate\`. Le configurazioni `Game Development` e
`Game Shipping` vengono compilate solo per verifica: i loro binari non
vanno copiati da nessuna parte, li rigenera il cooker al momento del
packaging del progetto finale.

---

## 4. Installare il plugin

**Su un solo progetto:** copia `Build\Spout2_DX12\` dentro
`YourProject\Plugins\Spout2_DX12\`, poi abilitalo da
**Edit → Plugins → Spout2_DX12**.

**Su tutti i progetti (a livello Engine):** copia lo stesso contenuto in

```
C:\Program Files\Epic Games\UE_5.8\Engine\Plugins\Marketplace\Spout2_DX12\
```

così ogni `.uproject` aperto con quell'Engine lo vedrà automaticamente in
**Edit → Plugins**, senza bisogno di averlo nella cartella `Plugins\` del
progetto. Richiede permessi di scrittura sulla cartella dell'Engine
(eventualmente esegui da amministratore).
