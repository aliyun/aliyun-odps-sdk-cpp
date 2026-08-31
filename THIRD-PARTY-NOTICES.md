# Third-Party Notices

本 SDK 在构建时按"系统库优先,否则下载源码编译"的策略使用以下第三方库。
发布包中若包含这些库的二进制产物,则同时受其各自许可证约束:

| 依赖 | 版本 | 许可证 |
| ---- | ---- | ------ |
| zlib | 1.2.11 | zlib License |
| OpenSSL | 1.1.1g | OpenSSL License(SSLeay/OpenSSL 双许可,类 BSD;Apache-2.0 自 OpenSSL 3.0 起采用) |
| zstd | 1.4.5 | BSD-3-Clause(亦提供 GPLv2 双许可,本构建采用 BSD) |
| lz4 | 1.9.3 | BSD-2-Clause |
| curl | 7.73.0 | curl License(MIT/X 衍生) |
| protobuf | 3.7.1 | BSD-3-Clause |
| Boost | 1.75.0 | Boost Software License 1.0 |
| Google Test | 1.10.0 | BSD-3-Clause |
| Apache Arrow | 1.0.0 | Apache-2.0(仅 `WITH_ARROW=ON` 时构建) |
| nlohmann/json | 3.11.3 | MIT(随源码内置于 `thirdparty/nlohmann/`,许可证见 `thirdparty/nlohmann/LICENSE.MIT`) |

各库的完整许可证文本随其源码发布;源码编译时位于构建目录
`build/ep/<dep>/src/` 下的对应源码包内。
