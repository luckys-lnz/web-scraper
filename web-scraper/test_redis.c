#include <hiredis/hiredis.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("Testing Redis connection...\n");
    
    // Try to connect to Redis
    redisContext *ctx = redisConnect("127.0.0.1", 6379);
    if (ctx == NULL || ctx->err) {
        if (ctx) {
            printf("Redis connection error: %s\n", ctx->errstr);
            redisFree(ctx);
        } else {
            printf("Cannot allocate redis context\n");
        }
        return 1;
    }
    
    // Try to send a PING command
    redisReply *reply = redisCommand(ctx, "PING");
    if (reply == NULL) {
        printf("Redis PING failed: %s\n", ctx->errstr);
        redisFree(ctx);
        return 1;
    }
    
    // Check the response
    if (reply->type == REDIS_REPLY_STATUS && strcmp(reply->str, "PONG") == 0) {
        printf("Redis connection successful!\n");
    } else {
        printf("Redis PING response invalid: type=%d, str=%s\n",
               reply->type, reply->str ? reply->str : "NULL");
    }
    
    // Clean up
    freeReplyObject(reply);
    redisFree(ctx);
    return 0;
} 