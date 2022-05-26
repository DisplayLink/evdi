// Copyright (c) 2022 DisplayLink (UK) Ltd.
pipeline {
    agent {
        dockerfile {
            filename 'src/Dockerfile'
        }
    }
    stages {
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
                    sh '''../src/ci/prepare_deb_package amd64'''
                    sh '''../src/ci/test_deb_package evdi-amd64.deb'''
                }
            }
        }
        stage ('Build evdi-armhf.deb') {
            steps {
                dir('publish') {
                    sh '''../src/ci/prepare_deb_package armhf'''
                    catchError(buildResult: 'SUCCESS', stageResult: 'FAILURE') {
                        sh '''../src/ci/test_deb_package evdi-armhf.deb'''
                    }
                }
            }
        }
        stage ('Build evdi-arm64.deb') {
            steps {
                dir('publish') {
                    sh '''../src/ci/prepare_deb_package arm64'''
                    catchError(buildResult: 'SUCCESS', stageResult: 'FAILURE') {
                        sh '''../src/ci/test_deb_package evdi-arm64.deb'''
                    }
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
    }
}
