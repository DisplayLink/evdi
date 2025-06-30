// Copyright (c) 2022 DisplayLink (UK) Ltd.
pipeline {
    agent {
        dockerfile {
            filename 'Dockerfile'
            label 'linux && big && docker'
        }
    }
    environment {
      EVDI_VERSION = sh(script: '''(. ./ci/deb_config; echo $evdi_version)''', returnStdout: true).trim()
    }
    stages {
        stage ('Init') {
            steps {
                sh '''
                  EVDI_VERSION_SHORT=$(echo ${EVDI_VERSION} | awk -F '.' '{printf("%d.%d", $1, $2)}')
                '''
                buildName "evdi-${EVDI_VERSION}-${BUILD_NUMBER}"
            }
        }
        stage ('Style check') {
            steps {
                sh '''make clean'''
                sh '''./ci/run_style_check'''
            }
        }
        stage ('Shellcheck') {
            steps {
              sh '''find ci module -type f -exec file '{}' + | grep shell | sed 's/:.*$//' | xargs shellcheck'''
            }
        }
        stage('BlackDuck Scan') {
            when { anyOf { branch 'main'; branch pattern: "release/v*" } }
            environment {
               DETECT_JAR_DOWNLOAD_DIR = "${env.WORKSPACE}/synopsys_download"
            }
            steps {
            script {
                  blackduck_detect detectProperties: "--detect.project.name='Evdi' --detect.project.version.name='${env.GIT_BRANCH}' --detect.excluded.directories=tmp,bd_evdi,synopsys_download --detect.output.path='${env.WORKSPACE}/bd_evdi'", downloadStrategyOverride: [$class: 'ScriptOrJarDownloadStrategy']
                  def buildUrl = "$BUILD_URL"
                  env.BLACKDUCK = sh(script: "curl -Lk '${buildUrl}/consoleText' | grep 'Black Duck Project BOM:'", returnStdout: true)
            }
            }
        }
        stage ('Build evdi-amd64.deb') {
            steps {
                dir('publish') {
                    sh '''../ci/prepare_deb_package amd64 ${BUILD_NUMBER}'''
                    sh '''../ci/test_deb_package evdi-${EVDI_VERSION}-${BUILD_NUMBER}_amd64.deb'''
                }
            }
        }
        stage ('Build evdi-armhf.deb') {
            steps {
                dir('publish') {
                    sh '''../ci/prepare_deb_package armhf ${BUILD_NUMBER}'''
                    sh '''../ci/test_deb_package evdi-${EVDI_VERSION}-${BUILD_NUMBER}_armhf.deb'''
                }
            }
        }
        stage ('Build evdi-arm64.deb') {
            steps {
                dir('publish') {
                    sh '''../ci/prepare_deb_package arm64 ${BUILD_NUMBER}'''
                    sh '''../ci/test_deb_package evdi-${EVDI_VERSION}-${BUILD_NUMBER}_arm64.deb'''
                }
            }
        }
        stage ('Build pyevdi') {
            steps {
                sh '''make clean'''
                sh '''make -C library'''
                sh '''make -C pyevdi'''
            }
        }
        stage('SonarQube Scan') {
            steps {
                sh '''#!/usr/bin/env bash
                make clean
                bear -- make all-with-rc-linux
                '''
                withSonarQubeEnv('SonarQube') {
                    sh '''
                        sonar-scanner \
                          -D sonar.cfamily.compile-commands=compile_commands.json \
                          -D sonar.projectVersion="${BUILD_NUMBER}" \
                          -D sonar.exclusions=bd_evdi/**
                    '''
                }
            }
        }
        stage ('Build against released kernels') {
            steps {
                sh '''#!/usr/bin/env bash
                set -e
                ./ci/build_against_kernel --repo-ci all'''
            }
        }
        stage ('Build against latest rc kernel') {
            steps { 
                sh '''#!/usr/bin/env bash
                set -e
                ./ci/build_against_kernel --repo-ci rc'''
                script {
                    env.KERNELS = sh(script: "./ci/build_against_kernel --list-kernels", returnStdout: true).trim()
                }
            }
        }
        stage ('Run KUnit tests') {
            steps {
                sh '''#!/usr/bin/env bash
                [ -d tmp/linux ] && (cd tmp/linux; git checkout -f master; git reset --hard origin/master)
                ./ci/run_kunit'''
            }
        }
        stage ('Publish') {
          when { anyOf { branch 'main'; branch pattern: "release/v*" } }
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
          when { anyOf { branch 'main'; branch pattern: "release/v*" } }
          steps {
            build (
                job: 'PPD-POSIX/promote build',
                parameters: [string(name: 'SERVER', value: 'DEVELOPMENT'),
                  string(name: 'buildName', value: "${env.JOB_NAME}".replaceAll('/', " :: ").replaceAll('%2F', " :: ")),
                  string(name: 'buildNumber', value: "${env.BUILD_NUMBER}")])
          }
        }
        stage ( 'Create note' )
        {
          when { anyOf { branch 'main' } }
          steps {
            build (
                job: 'PPD-POSIX/create release note/master',
                parameters: [string(name: 'kernels', value: env.KERNELS),
                  string(name: 'git_hash', value: env.GIT_COMMIT),
                  string(name: 'build_number', value: env.BUILD_NUMBER),
                  string(name: 'evdi_ver', value: env.EVDI_VERSION),
                  string(name: 'blackduck', value: env.BLACKDUCK),
                ])
          }
        }
    }
}
