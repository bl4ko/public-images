ARG SEERR_VERSION
FROM ghcr.io/seerr-team/seerr:${SEERR_VERSION}

USER root
RUN find /app/.next/static/chunks -type f -name '*.css' -exec sh -c 'printf "%s\n" "form[data-form-type=\"login\"]+div.mt-4{display:none!important}" >> "$1"' _ {} \;
USER node
