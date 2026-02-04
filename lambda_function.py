import json
import time
import boto3
import botocore.exceptions

TABLE_NAME = "wand-ws-connections"
WS_API_ENDPOINT = "https://jcdgiqkfsh.execute-api.us-east-1.amazonaws.com/production"

dynamodb = boto3.resource("dynamodb")
table = dynamodb.Table(TABLE_NAME)

apigw = boto3.client("apigatewaymanagementapi", endpoint_url=WS_API_ENDPOINT)

def lambda_handler(event, context):
    payload = {
        "ts": int(time.time()),
        "source": "iot",
        "data": event
    }
    message = json.dumps(payload, ensure_ascii=False).encode("utf-8")

    resp = table.scan(ProjectionExpression="connectionId")
    items = resp.get("Items", [])

    sent = 0
    stale = 0

    for it in items:
        cid = it["connectionId"]
        try:
            apigw.post_to_connection(ConnectionId=cid, Data=message)
            sent += 1
        except botocore.exceptions.ClientError as e:
            code = e.response.get("ResponseMetadata", {}).get("HTTPStatusCode")
            if code == 410:
                table.delete_item(Key={"connectionId": cid})
                stale += 1
            else:
                raise

    return {"statusCode": 200, "sent": sent, "stale_cleaned": stale}
