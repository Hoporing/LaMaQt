# LaMaQt

Qt6/QML 기반 Image Inpainting + Interactive Segmentation Application.  
SAM2로 객체를 선택하고 LaMa로 자연스럽게 제거합니다. 전처리/후처리를 GPU에서 수행하여 빠른 Inference가 가능합니다.

---

## 실행 화면

![Demo](docs/demo.gif)
<!-- Demo GIF 또는 Screenshot 추가 후 위 경로 업데이트 -->

---

## 주요 기능

- **Interactive Segmentation** — SAM2 Point Prompt 기반 클릭 한 번으로 Mask 생성
- **Image Inpainting** — LaMa Model을 이용한 자연스러운 객체 제거
- **Manual Mask** — Brush Tool로 직접 Mask 영역 지정
- **GPU 가속** — 전처리/후처리 전부 `cv::cuda::GpuMat` (D2D Zero-copy)
- **다중 SAM2 Model 지원** — tiny / small / base_plus / large 선택 가능
- **QML UI** — Source Panel + Workspace Panel + Theme 지원

---

## 기술 스택

| 분류 | 내용 |
|------|------|
| UI Framework | Qt 6.10.2, QML |
| 언어 | C++ (MSVC 2022 64bit) |
| Inference | ONNX Runtime 1.24+ (CUDA Execution Provider) |
| 영상처리 | OpenCV 4.13 (CUDA modules 포함) |
| GPU | CUDA 13.1, cuDNN 9.17 |
| Build | CMake, Qt Creator |

---

## 사전 요구사항

- [Qt 6.10.2](https://www.qt.io/download) (MSVC 2022 64bit)
- CUDA 13.1 + cuDNN 9.17
- ONNX Runtime 1.24+ (CUDA EP)
- OpenCV 4.13 (cuda modules 빌드 필요)
- Model 파일 다운로드 (아래 참고)

---

## Model 파일 준비

SAM2 ONNX Model을 `models/SAM_2/{variant}/` 경로에 배치합니다.

[samexporter](https://github.com/vietanhdev/samexporter)를 이용해 Export하거나 직접 준비:

```
models/
└── SAM_2/
    ├── tiny/
    │   ├── sam2_hiera_tiny.encoder.onnx
    │   └── sam2_hiera_tiny.decoder.onnx
    ├── small/  ...
    ├── base_plus/  ...
    └── large/  ...
```

LaMa ONNX Model (`lama.onnx`)은 [lama-cleaner](https://github.com/Sanster/lama-cleaner) 또는 export script로 생성합니다.

---

## 빌드 및 실행

```bash
# Qt Creator에서 프로젝트 열기
# LaMaQt/CMakeLists.txt 선택

# 또는 CLI (MSVC 환경에서)
cmake -B build -S . -DCMAKE_PREFIX_PATH=<Qt 경로>
cmake --build build --config Release
```

> **Note:** ONNX Runtime이 TensorRT 대신 사용된 이유 — LaMa의 FFC Block이 DFT Operator를 사용하며, TensorRT 10.x에서 미지원. ONNX Runtime CUDA EP는 DFT를 Native 지원.

---

## License

본 프로젝트 소스코드는 [MIT License](LICENSE)를 따릅니다.

사용된 오픈소스 라이브러리:

| Library | License |
|---------|---------|
| Qt 6 | LGPL v3 |
| ONNX Runtime | MIT |
| OpenCV | Apache 2.0 |
| LaMa (Model) | Apache 2.0 |
| SAM2 (Model) | Apache 2.0 |
