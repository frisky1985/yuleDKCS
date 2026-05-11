{{/*
Expand the name of the chart.
*/}}
{{- define "yuledkcs.name" -}}
{{- default .Chart.Name .Values.nameOverride | trunc 63 | trimSuffix "-" }}
{{- end }}

{{/*
Create a default fully qualified app name.
We truncate at 63 chars because some Kubernetes name fields are limited to this (by the DNS naming spec).
If release name contains chart name it will be used as a full name.
*/}}
{{- define "yuledkcs.fullname" -}}
{{- if .Values.fullnameOverride }}
{{- .Values.fullnameOverride | trunc 63 | trimSuffix "-" }}
{{- else }}
{{- $name := default .Chart.Name .Values.nameOverride }}
{{- if contains $name .Release.Name }}
{{- .Release.Name | trunc 63 | trimSuffix "-" }}
{{- else }}
{{- printf "%s-%s" .Release.Name $name | trunc 63 | trimSuffix "-" }}
{{- end }}
{{- end }}
{{- end }}

{{/*
Create chart name and version as used by the chart label.
*/}}
{{- define "yuledkcs.chart" -}}
{{- printf "%s-%s" .Chart.Name .Chart.Version | replace "+" "_" | trunc 63 | trimSuffix "-" }}
{{- end }}

{{/*
Common labels
*/}}
{{- define "yuledkcs.labels" -}}
helm.sh/chart: {{ include "yuledkcs.chart" . }}
{{ include "yuledkcs.selectorLabels" . }}
{{- if .Chart.AppVersion }}
app.kubernetes.io/version: {{ .Chart.AppVersion | quote }}
{{- end }}
app.kubernetes.io/managed-by: {{ .Release.Service }}
{{- end }}

{{/*
Selector labels
*/}}
{{- define "yuledkcs.selectorLabels" -}}
app.kubernetes.io/name: {{ include "yuledkcs.name" . }}
app.kubernetes.io/instance: {{ .Release.Name }}
{{- end }}

{{/*
Create the name of the service account to use
*/}}
{{- define "yuledkcs.serviceAccountName" -}}
{{- if .Values.serviceAccount.create }}
{{- default (include "yuledkcs.fullname" .) .Values.serviceAccount.name }}
{{- else }}
{{- default "default" .Values.serviceAccount.name }}
{{- end }}
{{- end }}

{{/*
Create the name of the config map to use
*/}}
{{- define "yuledkcs.configMapName" -}}
{{- default (printf "%s-config" (include "yuledkcs.fullname" .)) .Values.existingConfigMap }}
{{- end }}

{{/*
Create the name of the secret to use
*/}}
{{- define "yuledkcs.secretName" -}}
{{- default (printf "%s-secrets" (include "yuledkcs.fullname" .)) .Values.existingSecret }}
{{- end }}

{{/*
Return the proper image name
*/}}
{{- define "yuledkcs.image" -}}
{{- $registryName := .Values.global.imageRegistry -}}
{{- $repositoryName := .Values.image.repository -}}
{{- $tag := .Values.image.tag | default .Chart.AppVersion -}}
{{- printf "%s/%s:%s" $registryName $repositoryName $tag -}}
{{- end }}

{{/*
Return the proper Docker Image Registry Secret Names
*/}}
{{- define "yuledkcs.imagePullSecrets" -}}
{{- include "common.images.pullSecrets" (dict "images" (list .Values.image) "global" .Values.global) -}}
{{- end }}

{{/*
Check if autoscaling is enabled
*/}}
{{- define "yuledkcs.autoscaling.enabled" -}}
{{- if .Values.autoscaling.enabled }}true{{ else }}false{{ end -}}
{{- end }}

{{/*
Get the ingress API version
*/}}
{{- define "yuledkcs.ingress.apiVersion" -}}
{{- if semverCompare ">=1.19-0" .Capabilities.KubeVersion.GitVersion -}}
{{- print "networking.k8s.io/v1" -}}
{{- else -}}
{{- print "networking.k8s.io/v1beta1" -}}
{{- end -}}
{{- end }}

{{/*
Return true if cert-manager required annotations for TLS signed certificates are set in the Ingress annotations
Ref: https://cert-manager.io/docs/usage/ingress/#supported-annotations
*/}}
{{- define "yuledkcs.ingress.certManagerRequest" -}}
{{ if or (hasKey . "cert-manager.io/cluster-issuer") (hasKey . "cert-manager.io/issuer") }}
    {{- true -}}
{{- end -}}
{{- end -}}

{{/*
Compile all warnings into a single message.
*/}}
{{- define "yuledkcs.validateValues" -}}
{{- $messages := list -}}
{{- $messages := append $messages (include "yuledkcs.validateValues.image" .) -}}
{{- $messages := without $messages "" -}}
{{- $message := join "\n" $messages -}}
{{- if $message -}}
{{- printf "\nVALUES VALIDATION:\n%s" $message | fail -}}
{{- end -}}
{{- end -}}

{{/* Validate values of yuledkcs - Image */}}
{{- define "yuledkcs.validateValues.image" -}}
{{- if not .Values.image.repository -}}
yuledkcs: image.repository
    You must provide a valid image repository.
{{- end -}}
{{- end -}}
