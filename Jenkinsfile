// Copyright (c) 2022 DisplayLink (UK) Ltd.
pipeline {
    agent {
        dockerfile {
            filename 'src/Dockerfile'
        }
    }
    environment {
      GIT_DESC = sh(script: '''(cd src; git describe --tags --match=v*)''', returnStdout: true).trim()
      EVDI_VERSION = sh(script: '''(cd src; . ./ci/deb_config; echo $evdi_version)''', returnStdout: true).trim()
    }
    stages {
        stage ('Init') {
            steps {
                dir('src') {
                    sh '''./ci/check_version'''
                    sh '''bash -c "[[ evdi-${EVDI_VERSION} ==  ${JOB_BASE_NAME}* ]]"'''
                }
                buildName "evdi-${EVDI_VERSION}-${BUILD_NUMBER}"
                buildDescription "#${BUILD_NUMBER}-${GIT_DESC}"
            }
        }
        stage ('Style check') {
            steps {
                dir('src') {
                    sh '''make clean'''
                    sh '''./ci/run_style_check'''
                }
            }
        }
        stage ('Shellcheck') {
            steps {
                dir('src') {
                  sh '''shellcheck ./ci/*'''
                }
            }
        }
        stage ('Build evdi-amd64.deb') {
            steps {
                dir('publish') {
                    sh '''../src/ci/prepare_deb_package amd64 ${BUILD_NUMBER}'''
                    sh '''../src/ci/test_deb_package evdi-${EVDI_VERSION}-${BUILD_NUMBER}_amd64.deb'''
                }
            }
        }
        stage ('Build evdi-armhf.deb') {
            steps {
                dir('publish') {
                    sh '''../src/ci/prepare_deb_package armhf ${BUILD_NUMBER}'''
                    sh '''../src/ci/test_deb_package evdi-${EVDI_VERSION}-${BUILD_NUMBER}_armhf.deb'''
                }
            }
        }
        stage ('Build evdi-arm64.deb') {
            steps {
                dir('publish') {
                    sh '''../src/ci/prepare_deb_package arm64 ${BUILD_NUMBER}'''
                    sh '''../src/ci/test_deb_package evdi-${EVDI_VERSION}-${BUILD_NUMBER}_arm64.deb'''
                }
            }
        }
	stage ('Build pyevdi') {
            steps {
                dir('src') {
                    sh '''make clean'''	
                    sh '''make -C library'''
                    sh '''make -C pyevdi'''
                }
            }
        }
        stage ('Build against released kernels') {
            steps {
                dir('src') {
                    sh '''./ci/build_against_kernel --repo-ci all'''
                }
            }
        }
        stage ('Build against latest rc kernel') {
            steps {
                dir('src') {
                    sh '''./ci/build_against_kernel --repo-ci rc'''
                }
            }
        }
        stage ('Publish') {
          steps {
            rtBuildInfo (
                captureEnv: true,
                maxBuilds: 2048,
                maxDays: 1095, // 3 years
                deleteBuildArtifacts: true)
            rtUpload (
              serverId: 'Artifactory',
                  spec: '''{
                    "files": [
                      {
                      "pattern": "publish/evdi-${EVDI_VERSION}-${BUILD_NUMBER}_amd64.deb",
                      "target": "swbuilds-scratch/linux/evdi/amd64/${BUILD_DISPLAY_NAME}_amd64.deb"
                      },
                      {
                      "pattern": "publish/evdi-${EVDI_VERSION}-${BUILD_NUMBER}_armhf.deb",
                      "target": "swbuilds-scratch/linux/evdi/armhf/${BUILD_DISPLAY_NAME}_armhf.deb"
                      },
                      {
                      "pattern": "publish/evdi-${EVDI_VERSION}-${BUILD_NUMBER}_arm64.deb",
                      "target": "swbuilds-scratch/linux/evdi/arm64/${BUILD_DISPLAY_NAME}_arm64.deb"
                      }
                    ]}''',
                  failNoOp: true)
            rtPublishBuildInfo (
              serverId: 'Artifactory')
          }
        }
        stage ( 'Run job: promote build' )
        {
          steps {
            build (
                job: 'PPD-POSIX/promote build',
                parameters: [string(name: 'SERVER', value: 'DEVELOPMENT'),
                  string(name: 'buildName', value: "${env.JOB_NAME}".replaceAll('/', " :: ")),
                  string(name: 'buildNumber', value: "${env.BUILD_NUMBER}")])
          }
        }
    }
}
