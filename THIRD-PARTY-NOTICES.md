# 제3자 구성요소의 고지 (Third-party notices)

NlToyBox 는 GNU Affero General Public License 버전 3 으로 배포한다(`LICENSE`).
빌드한 모듈(`NlToyBox.dll`)에는 아래의 제3자 코드가 함께 들어간다. 그 저작권 고지와 허가 조건을 여기에 싣는다.
모듈을 배포할 때는 이 파일과 `LICENSE`를 함께 넣는다.

## YYToolkit — AGPL-3.0

- 출처: https://github.com/AurieFramework/YYToolkit (서브모듈 `external/YYToolkit`, `experimental` 브랜치의 커밋 `d5cc0078`)
- 모듈에 들어가는 것: `YYToolkit/source/YYTK/Shared/YYTK_Shared_Types.cpp`와 그 헤더들
- 라이선스: GNU Affero General Public License v3.0. 전문은 이 저장소의 `LICENSE`와 같다(서브모듈의 `LICENSE`).

## Dear ImGui — MIT

- 출처: https://github.com/ocornut/imgui (서브모듈 `external/imgui`, v1.91.9)
- 모듈에 들어가는 것: `imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp`, `backends/imgui_impl_dx11.cpp`, `backends/imgui_impl_win32.cpp`와 그 헤더들

```
The MIT License (MIT)

Copyright (c) 2014-2025 Omar Cornut

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

### Dear ImGui 에 들어 있는 것

Dear ImGui 의 소스가 아래의 것을 품고 있어 모듈에도 함께 들어간다. 고지는 Dear ImGui 의 소스에 적힌 대로다.

- **ProggyClean.ttf** (기본 글꼴의 자료. `imgui_draw.cpp`) — Copyright (c) 2004, 2005 Tristan Grimmer. MIT license.
  모듈은 Windows 의 `malgun.ttf`를 찾지 못했을 때만 이 글꼴을 쓴다.
- **stb_truetype, stb_rect_pack, stb_textedit** (`imstb_truetype.h`, `imstb_rectpack.h`, `imstb_textedit.h`) — Copyright (c) 2017 Sean Barrett.
  MIT License 와 Public Domain (www.unlicense.org) 가운데 고르는 이중 라이선스.

위 둘의 MIT 허가 조건은 위에 실은 MIT License 의 글과 같다(저작권자의 줄만 다르다).

## 모듈에 들어가지 않는 것

아래는 모듈이 돌 때 필요하지만 이 저장소와 배포 묶음에 들어 있지 않다. `tools/setup-aurie.ps1`이 제 출처에서 받는다(`tools/pins.json`).

- **Aurie** v2.0.2 (`AurieCore.dll`, `AuriePatcher.exe`) — https://github.com/AurieFramework/Aurie
- **YYToolkit** v5.0.0c (`YYToolkit.dll`) — https://github.com/AurieFramework/YYToolkit

모듈은 Microsoft Visual C++ 런타임(`MSVCP140.dll`, `VCRUNTIME140.dll`, `VCRUNTIME140_1.dll`)과 Windows 의 `D3DCOMPILER_47.dll`에 기댄다. 이것들도 묶음에 넣지 않는다.

게임 Norland 와 그 파일은 Long Jaunt 의 저작물이다. 이 저장소와 배포 묶음에는 게임의 파일이 들어 있지 않다.
