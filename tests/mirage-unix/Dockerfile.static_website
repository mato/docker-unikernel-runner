# Runtime container for Mirage 'static_website' example.

FROM mir-runner
# mirage-http requires libgmp
RUN apk add --update --no-cache gmp
ADD mir-static_website.tar.gz /unikernel/

CMD ["/runtime/runner", "unix", "/unikernel/www"]
