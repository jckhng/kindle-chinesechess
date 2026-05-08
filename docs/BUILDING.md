# Building Exact Chinese Chess

## Requirements

- Docker.
- ARM binfmt support if your Docker setup does not already run ARM containers.

Install ARM binfmt support on Linux with:

```bash
docker run --privileged --rm tonistiigi/binfmt --install arm
```

## Build And Package

Initialize the pinned Pikafish source if you want to rebuild the engine:

```bash
git submodule update --init --recursive
```

Build the Kindle app and package the current contents:

```bash
./docker_rebuild.sh
```

Build Pikafish and repackage with the engine and NNUE:

```bash
./build_pikafish.sh
./package_extension.sh
```

The persistent builder is:

```text
image:     exact-chinesechess-armhf-build:bullseye
container: exact-chinesechess-armhf-builder
```

Build outputs:

```text
exact-chinesechess
smoke-test
release/exact-chinesechess-extension.zip
```

## Build Without Packaging

```bash
EXACT_CHINESECHESS_PACKAGE=0 ./docker_rebuild.sh
```

## Builder Shell

```bash
./docker_shell.sh
```

Inside the container:

```bash
make clean
make exact-chinesechess smoke-test
./smoke-test
```

If you move the repository, recreate the persistent container:

```bash
docker rm -f exact-chinesechess-armhf-builder
./docker_rebuild.sh
```
