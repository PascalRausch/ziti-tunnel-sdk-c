# use ziti-builder:v1 (basded on ubuntu focal) for older glibc (2.27)
FROM openziti/ziti-builder:v1

COPY ./entrypoint.sh /root/
RUN chmod +x /root/entrypoint.sh
ENTRYPOINT [ "/root/entrypoint.sh" ]
