# aliyun-odps-sdk-cpp

阿里云 MaxCompute(原 ODPS)C++ SDK。

## 环境要求

- **编译器:GCC ≥ 4.9.2**。构建只使用系统/本机已有的编译器,不会自动下载编译器。
  低于 4.9.2 的 GCC 会在配置阶段直接报错退出;非 GNU 编译器(如 Clang)会给出警告,不保证支持。
- **CMake ≥ 3.22**。
- 如需指定/切换编译器,可在配置时显式传入(可选):
  - `-DCMAKE_CXX_COMPILER=<path-to-g++>` 与 `-DCMAKE_C_COMPILER=<path-to-gcc>`;
  - 例如用旧版 gcc 做编译器下限验证:`-DCMAKE_CXX_COMPILER=/path/to/gcc-4.9.2/bin/g++`(仅为示例路径)。

> 说明:GCC ≥ 5.0.0 时构建会自动注入 `-D_GLIBCXX_USE_CXX11_ABI=0`,并对所有源码编译的 C++ 依赖注入相同标志,以保持 ABI 一致。

## 快速构建

```shell
mkdir build && cd build
cmake ..
make -j
```

如需固定编译器(示例):

```shell
mkdir build && cd build
cmake .. -DCMAKE_CXX_COMPILER=/path/to/gcc-4.9.2/bin/g++ \
         -DCMAKE_C_COMPILER=/path/to/gcc-4.9.2/bin/gcc
make -j
```

产物位于 `build/lib`(库文件)与 `build/bin`(可执行文件,如测试程序)。

## 构建选项

以下选项在顶层 `CMakeLists.txt` 中通过 `option()` 定义,默认值以代码为准:

| 选项 | 说明 | 默认值 |
| ---- | ---- | ------ |
| `WITH_ARROW` | 是否构建 Apache Arrow 支持。基于 Arrow 的 stream 类接口需 `-DWITH_ARROW=ON` 才可用 | `OFF` |
| `WITH_PERF_TOOL` | 是否构建 `perf_tool` 可执行程序(开启后会额外编译 Boost 的 `program_options` 库) | `OFF` |
| `WITH_RANDOM_UUID` | 使用随机生成的 UUID 替代 Boost 提供的实现 | `OFF` |
| `ODPS_WERROR` | 是否启用 `-Werror` | `ON` |
| `ODPS_BUILD_TESTS` | 是否构建测试 | `ON` |
| `ODPS_BUILD_EXAMPLES` | 是否构建示例 | `ON` |
| `ODPS_FORCE_FETCH_DEPS` | 强制忽略系统库、全部下载源码编译 | `OFF` |

示例:启用 Arrow 支持并关闭示例:

```shell
cmake .. -DWITH_ARROW=ON -DODPS_BUILD_EXAMPLES=OFF
```

## 依赖管理策略

依赖统一在 `cmake/OdpsDeps.cmake` 及 `cmake/deps/*.cmake` 中处理,策略为:

1. **系统库优先**:优先通过 `find_package` / `find_library` 查找系统已安装的库;
2. **找不到则下载源码并本地编译**:以 `ExternalProject` 方式下载源码、编译并安装到构建目录内的统一前缀 `build/deps_install`(不会污染系统目录);
3. **动态链接**:源码编译的依赖默认产出共享库(`.so`)。

涉及的依赖及版本:

| 依赖 | 版本 | 获取方式 |
| ---- | ---- | -------- |
| zlib | 1.2.11 | 系统优先,否则源码编译 |
| OpenSSL | 1.1.1g | 系统优先,否则源码编译 |
| zstd | 1.4.5 | 系统优先,否则源码编译 |
| lz4 | 1.9.3 | 系统优先,否则源码编译 |
| curl | 7.73.0 | 系统优先,否则源码编译 |
| protobuf | 3.7.1 | 源码编译 |
| Boost | 1.75.0 | 源码编译(默认仅头文件;`WITH_PERF_TOOL=ON` 时额外编译 `program_options`) |
| GTest | 1.10.0 | 源码编译 |
| Arrow | 1.0.0 | 可选,`WITH_ARROW=ON` 时源码编译 |
| nlohmann/json | 3.11.3 | 随源码内置(`thirdparty/nlohmann/`,header-only,MIT) |

### 覆盖依赖来源

每个依赖都提供 `<DEP>_DEP_URL` 与 `<DEP>_DEP_SHA256` 两个缓存变量,可在配置时覆盖下载地址与校验和(例如离线/内网环境使用镜像源):

```shell
cmake .. \
  -DZLIB_DEP_URL=https://your-mirror/zlib-1.2.11.tar.gz \
  -DZLIB_DEP_SHA256=<sha256>
```

`<DEP>` 取值为:`ZLIB`、`OPENSSL`、`ZSTD`、`LZ4`、`CURL`、`PROTOBUF`、`BOOST`、`GTEST`、`ARROW`。
即对应变量为 `ZLIB_DEP_URL` / `ZLIB_DEP_SHA256`、`OPENSSL_DEP_URL` / `OPENSSL_DEP_SHA256`,依此类推。

如需强制忽略系统库:

- 全部依赖:`-DODPS_FORCE_FETCH_DEPS=ON`;
- 单个依赖:`-DFORCE_FETCH_ZLIB=ON`、`-DFORCE_FETCH_OPENSSL=ON`、`-DFORCE_FETCH_ZSTD=ON`、`-DFORCE_FETCH_LZ4=ON`、`-DFORCE_FETCH_CURL=ON`(仅对支持系统查找的依赖有效)。

## 打包

```shell
cd build
make install
```

`make install` **不会向系统目录安装任何内容**,只会在仓库内的 `package/` 目录生成发布包:

```
package/aliyun_odps_sdk_cpp_<时间戳>_release.tar.gz
```

包内含:`lib/`(SDK 的动态/静态库及依赖 `.so`)、`include/`(公共头文件)、`BUILD_INFO`(构建信息)、`LICENSE`/`NOTICE`/`THIRD-PARTY-NOTICES.md`(许可证与第三方声明)。

> 注意:发布包**不包含**编译器运行时库(`libstdc++.so.6`/`libgcc_s.so.1`),目标机器需自行提供兼容的 libstdc++(即 GCC 版本不低于构建所用编译器)。另外,若构建时使用了系统库(系统库优先策略),目标机器上还需存在兼容的同名系统库;构建时找到的系统库版本按原样接受,不强制版本下限。

## 测试与示例配置

测试与示例的连接信息统一从 `conf/testing.conf` 读取(仓库不收录该文件,避免凭证泄露)。首次使用:

```shell
cp conf/testing.conf.sample conf/testing.conf
```

按模板填入实际值:`AccessId`、`AccessKey`、`OdpsEndpoint`、`TunnelEndpoint`、`ProjectName` 等(测试还会用到 `TestAccessId`、`TestAccessKey`、`TestUser`、`SchemaEnabledProject`)。

测试可执行文件为 `build/bin/all_testing`:

```shell
./build/bin/all_testing
```

示例位于 `example/` 目录(core / tunnel / max_storage_api),编译产物在 `build/bin`。所有示例的 endpoint、凭证、默认项目均从 `conf/testing.conf` 读取,命令行只接收数据类参数(表名、SQL、instance id 等);请在仓库根目录下运行,例如:

```shell
./build/bin/table_meta <table_name> [schema]
```

> 提示:`all_testing` 链接的是源码编译出的依赖共享库,若运行时找不到 `.so`,请设置 `LD_LIBRARY_PATH` 指向 `build/deps_install/lib` 与 `build/lib`。
>
> 此外,`all_testing`(以及任何由较新 GCC 构建出的二进制)运行时还需能加载构建所用编译器的 `libstdc++.so.6`/`libgcc_s.so.1`,即需要不低于构建编译器版本的 GCC 所提供的 libstdc++,可将编译器的 `lib64` 目录一并加入 `LD_LIBRARY_PATH`。在部分机器上,直接对系统 `/lib64/libstdc++.so.6` 执行 `ldd` 会打印 `version 'GLIBCXX_...' not found` 字样,原因正是如此——这是运行时搜索路径问题,不是构建缺陷。

## 许可证

本项目基于 [Apache License 2.0](LICENSE) 发布。构建与运行所涉及的第三方库许可见 [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md)。
