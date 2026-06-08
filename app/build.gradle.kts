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
            ndk {
                abiFilters.addAll(setOf("arm64-v8a"))
            }
        }
        ndk {
            abiFilters.addAll(setOf("arm64-v8a"))
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
}

dependencies {
    testImplementation(libs.junit)
    androidTestImplementation(libs.ext.junit)
}