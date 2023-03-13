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
      PUBLISH = sh(script: '''bash -c "[[ ${GIT_BRANCH} =~ ^origin/(devel$|github_devel$|release//*) ]] && echo true || echo false"
                           ''', returnStdout: true).trim()
    }
    stages {
        stage ('Init') {
            steps {
                dir('src') {
                    sh '''
                    ./ci/check_version
                    EVDI_VERSION_SHORT=$(echo ${EVDI_VERSION} | awk -F '.' '{printf("%d.%d", $1, $2)}')
                    bash -c "[[ ${JOB_BASE_NAME} == *evdi-${EVDI_VERSION_SHORT}* ]]"
                    '''
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
                  sh '''find ci -type f -exec file '{}' + | grep shell | sed 's/:.*$//' | xargs shellcheck'''
                }
            }
        }
        stage('BlackDuck Scan') {
            when { environment name: 'PUBLISH', value: 'true' }
            environment {
               DETECT_JAR_DOWNLOAD_DIR = "${env.WORKSPACE}/synopsys_download"
            }
            steps {
                    dir("src") {
                      synopsys_detect detectProperties: "--detect.project.name='Evdi' --detect.project.version.name='${env.GIT_BRANCH}' --detect.blackduck.signature.scanner.exclusion.patterns=/tmp/ --detect.output.path='${env.WORKSPACE}/bd_evdi'", downloadStrategyOverride: [$class: 'ScriptOrJarDownloadStrategy']
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
          when { environment name: 'PUBLISH', value: 'true' }
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
          when { environment name: 'PUBLISH', value: 'true' }
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
