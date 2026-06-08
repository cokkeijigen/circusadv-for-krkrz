plugins {
    alias(libs.plugins.android.application)
}

android {
    namespace = "ss.colytitse.kr2patch"
    compileSdk {
        version = release(36)
    }

    defaultConfig {
        applicationId = "ss.colytitse.kr2patch"
        minSdk = 29
        targetSdk = 36
        versionCode = 1
        versionName = "1.0"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        externalNativeBuild {
            cmake {
                val vcpkgRoot = System.getenv("VCPKG_ROOT")
                if (!vcpkgRoot.isNullOrEmpty()) {
                    arguments("-DVCPKG_ROOT=$vcpkgRoot")
                } else {
                    logger.warn("⚠️ Warning: VCPKG_ROOT environment variable not detected!")
                }
                ndk {
                    abiFilters.addAll(setOf("arm64-v8a"))
                }
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
    packaging {
        jniLibs {
            excludes += listOf("lib/*/libdobby.so")
        }
    }
    buildFeatures {
        viewBinding = true
    }
    ndkVersion = "28.2.13676358"


    tasks.register("compile_jni_release") {
        group = "build"

        val stripReleaseTask = tasks.named("stripReleaseDebugSymbols")
        dependsOn(stripReleaseTask)
        doLast {
            val stripOutDir = stripReleaseTask.get().outputs.files.files.firstOrNull()
            if (stripOutDir?.exists() == true) {
                val targetDir = file("${project.layout.buildDirectory.get()}/outputs/libs/release")
                stripOutDir.resolve("lib").copyRecursively(targetDir, overwrite = true)
            }
        }
    }

    tasks.register("compile_jni_debug") {
        group = "build"

        val stripDebugTask = tasks.named("stripDebugDebugSymbols")
        dependsOn(stripDebugTask)
        doLast {
            val stripOutDir = stripDebugTask.get().outputs.files.files.firstOrNull()
            if (stripOutDir?.exists() == true) {
                val targetDir = file("${project.layout.buildDirectory.get()}/outputs/libs/debug")
                stripOutDir.resolve("lib").copyRecursively(targetDir, overwrite = true)
            }
        }
    }

    tasks.register("compile_jni_all")
    {
        group = "build"
        dependsOn(tasks.named("compile_jni_release"))
        dependsOn(tasks.named("compile_jni_debug"))
    }
}

dependencies {
    testImplementation(libs.junit)
    androidTestImplementation(libs.ext.junit)
}